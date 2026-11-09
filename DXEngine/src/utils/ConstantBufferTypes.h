#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <memory>
#include <array>
#include <cstddef>
#include <cstdint>

namespace DXEngine {

    struct TransfomBufferData
    {
        DirectX::XMMATRIX WVP;
        DirectX::XMMATRIX Model;
        DirectX::XMMATRIX View;
        DirectX::XMMATRIX Projection;
        DirectX::XMFLOAT3 cameraPosition;
        float time;
    };

    struct BoneMatrixBuffer
    {
        DirectX::XMFLOAT4X4 boneMatrices[128];
    };

    /// <summary>
    /// /Light Buffers
    /// </summary>
    struct DirectionalLightGPU
    {
        DirectX::XMFLOAT3 direction;
        float intensity;
        DirectX::XMFLOAT3 color;
        float shadowMapIndex; // -1 if no shadows
        DirectX::XMFLOAT4 cascadeSplits; // For CSM
        DirectX::XMMATRIX shadowMatrices[4]; // Up to 4 cascade levels
    };

    // ------------------------------------------------------------------
    // LOCAL lights (point + spot): one element of the structured buffer `localLights` (SRV_Lights).
    //
    // 64 bytes, laid out in four 16-byte rows. THE HLSL STRUCT IN SandBox/assets/shaders/common.hlsli MUST MATCH
    // FIELD FOR FIELD (structured buffers use plain C packing, so a float3 + float pair fills one row).
    // The size and every offset are checked below: change a field here and the build tells you.
    //
    //   color        linear RGB already multiplied by the intensity (the shader never needs them apart)
    //   direction    spot: the way the light points (unit); point: unused
    //   spotScale / spotOffset
    //                the spot cone as  saturate(cos(angle) * spotScale + spotOffset):  0 at the outer cone,
    //                1 at the inner cone, no division in the shader and no NaN when inner == outer
    //   shadowIndex  -1 = casts no shadow (the shadow milestone fills it in)
    // ------------------------------------------------------------------
    enum GpuLightType : uint32_t
    {
        GpuLight_Directional = 0,   // lives in SceneLightData, not in the structured buffer
        GpuLight_Point       = 1,
        GpuLight_Spot        = 2,
    };

    struct LightGPU
    {
        DirectX::XMFLOAT3 position;
        float             range;          // distance where the light has fully faded

        DirectX::XMFLOAT3 color;
        uint32_t          type;           // GpuLightType

        DirectX::XMFLOAT3 direction;
        int32_t           shadowIndex;

        float             spotScale;
        float             spotOffset;
        float             padding0;       // explicit: keeps the element at 64 bytes (room for flags later)
        float             padding1;
    };
    static_assert(sizeof(LightGPU) == 64, "LightGPU must stay 64 bytes (4 x float4) to match the HLSL struct");
    static_assert(offsetof(LightGPU, position)    == 0  && offsetof(LightGPU, range)       == 12 &&
                  offsetof(LightGPU, color)       == 16 && offsetof(LightGPU, type)        == 28 &&
                  offsetof(LightGPU, direction)   == 32 && offsetof(LightGPU, shadowIndex) == 44 &&
                  offsetof(LightGPU, spotScale)   == 48 && offsetof(LightGPU, spotOffset)  == 52,
                  "LightGPU field offsets no longer match the HLSL struct");

    // Constant buffer b3: the environment, the DIRECTIONAL lights, and how many local lights the structured buffer holds.
    // Directional lights stay here: there are only a few, every pixel needs all of them (nothing to cull), and the
    // shadow milestone attaches cascade matrices to them.
    struct SceneLightData
    {
        static constexpr uint32_t MAX_DIRECTIONAL_LIGHTS = 4;

        // Light counts
        uint32_t directionalLightCount;
        uint32_t localLightCount;     // valid elements of the local light buffer
        uint32_t padding[2];

        // Global lighting parameters
        DirectX::XMFLOAT3 ambientColor;
        float ambientIntensity;

        // IBL parameters
        float iblIntensity;
        float exposure;
        float gamma;
        float padding2;

        // Light arrays
        std::array<DirectionalLightGPU, MAX_DIRECTIONAL_LIGHTS> directionalLights;
    };

    

    // Constant buffer b7: how a pixel finds its light cluster. The cluster grid divides the view frustum into
    // gridX x gridY screen tiles and gridZ depth slices; slices grow exponentially with distance.
    //   tile   = pixel * tilesPerPixel
    //   slice  = floor(ln(viewZ) * sliceScale + sliceBias)
    // MIRRORED by cbuffer ClusterData in common.hlsli; the size is checked here.
    struct ClusterConstants
    {
        uint32_t gridX;
        uint32_t gridY;
        uint32_t gridZ;
        uint32_t mode;             // LightCullingMode: 0 = every pixel loops over all lights, 1 = clustered, 2 = heat map

        float tilesPerPixelX;      // gridX / screen width
        float tilesPerPixelY;      // gridY / screen height
        float sliceScale;          // gridZ / ln(far / near)
        float sliceBias;           // -ln(near) * sliceScale
    };
    static_assert(sizeof(ClusterConstants) == 32, "ClusterConstants must stay 32 bytes (2 x float4) to match the HLSL cbuffer");

    struct UIConstantBuffer
    {
        DirectX::XMMATRIX projection;
        float screenWidth;
        float screenHeight;
        float time;
        float padding;
    };


    enum BindSlot : uint32_t
    {
        CB_Transform = 0,
        CB_Bones = 1,
        CB_Material = 2,
        CB_Scene_Lights = 3,
        CB_UI = 4,
        CB_Shadow_Data = 5,        // For future shadow system
        CB_Post_Process = 6,       // For post-processing effects
        CB_Cluster_Data = 7        // Light cluster grid (ClusterConstants)


    };

    // Shader-resource (t#) slots of ENGINE-WIDE buffers. t0..t31 belong to the material textures
    // (TextureSlot::MaxTextureSlots), so anything the renderer binds for every draw starts above that.
    enum SrvSlot : uint32_t
    {
        SRV_Lights         = 32,    // StructuredBuffer<LightGPU> localLights
        SRV_ClusterRanges  = 33,    // StructuredBuffer<uint2>    clusterRanges        {offset, count} per cluster
        SRV_LightIndices   = 34,    // StructuredBuffer<uint>     clusterLightIndices  the lights of every cluster
    };
}