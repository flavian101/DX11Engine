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
        float receiveShadows;      // 1 = this draw looks the shadow maps up, 0 = it does not (MeshRendererComponent::receivesShadows)
        float padding[3];
    };
    static_assert(sizeof(TransfomBufferData) == 4 * 64 + 32, "TransfomBufferData must match cbuffer TransformBuffer in common.hlsli");

    struct BoneMatrixBuffer
    {
        DirectX::XMFLOAT4X4 boneMatrices[128];
    };

    /// <summary>
    /// /Light Buffers
    /// </summary>
    // 64 bytes (four 16-byte rows). MIRRORED by DirectionalLight in common.hlsli.
    //
    // The cascade matrices used to live here (4 x 64 bytes per light, never filled). They now live in ONE structured
    // buffer with every other shadow view (ShadowViewGPU), so a directional light only says WHERE its cascades start.
    //   shadowViewIndex  index of cascade 0 in that buffer; the cascades follow each other. -1 = no shadow.
    //   cascadeSplits    view-space depth at the FAR edge of cascade 0..3 (unused entries repeat the last one)
    //   shadowParams     x = cascade count, y = depth where the fade-out of the last cascade starts
    struct DirectionalLightGPU
    {
        DirectX::XMFLOAT3 direction;
        float intensity;
        DirectX::XMFLOAT3 color;
        int32_t shadowViewIndex;
        DirectX::XMFLOAT4 cascadeSplits;
        DirectX::XMFLOAT4 shadowParams;
    };
    static_assert(sizeof(DirectionalLightGPU) == 64, "DirectionalLightGPU must stay 64 bytes to match the HLSL struct");
    static_assert(offsetof(DirectionalLightGPU, shadowViewIndex) == 28 && offsetof(DirectionalLightGPU, cascadeSplits) == 32 &&
                  offsetof(DirectionalLightGPU, shadowParams) == 48, "DirectionalLightGPU offsets no longer match the HLSL struct");

    // ------------------------------------------------------------------
    // SHADOWS. Every shadow view (a directional cascade, a spot light's view, one face of a point light) is ONE element
    // of the structured buffer `shadowViews` (SRV_ShadowViews). 96 bytes, six rows. MIRRORED by ShadowView in common.hlsli.
    //
    //   row0..row3   the world -> light clip matrix, exactly as DirectXMath stores it (row vectors: clip = v * M).
    //                NOT transposed: the shader multiplies by the ROWS itself (r0*v.x + r1*v.y + r2*v.z + r3*v.w), so
    //                nothing depends on HLSL's matrix packing rule.
    //   atlasRect    this view's tile in the atlas, in UV units: xy = offset, zw = size
    //   params       x = normal offset in TEXELS, y = world size of one texel (perspective: at distance 1),
    //                z = constant depth bias in NDC depth, w = 0 orthographic (cascade) / 1 perspective (spot, point face)
    // ------------------------------------------------------------------
    struct ShadowViewGPU
    {
        DirectX::XMFLOAT4X4 viewProj;
        DirectX::XMFLOAT4   atlasRect;
        DirectX::XMFLOAT4   params;
    };
    static_assert(sizeof(ShadowViewGPU) == 96, "ShadowViewGPU must stay 96 bytes to match the HLSL struct");
    static_assert(offsetof(ShadowViewGPU, atlasRect) == 64 && offsetof(ShadowViewGPU, params) == 80, "ShadowViewGPU offsets changed");

    // Constant buffer b5. MIRRORED by cbuffer ShadowData in common.hlsli.
    struct ShadowConstants
    {
        float    atlasSize;        // in texels (square)
        float    invAtlasSize;
        uint32_t pcfRadius;
        uint32_t enabled;          // 0 = every lookup returns "lit" (shadows off)
    };
    static_assert(sizeof(ShadowConstants) == 16, "ShadowConstants must stay 16 bytes to match the HLSL cbuffer");

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
    //   shadowIndex  -1 = casts no shadow.  Spot: index of its one view in the shadow view buffer.  Point: index of its
    //                FIRST of six faces (+X -X +Y -Y +Z -Z, in this order: the shader picks the face from the direction).
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
        CB_Shadow_Data = 5,        // ShadowConstants
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
        SRV_ShadowAtlas    = 35,    // Texture2D<float>           shadowAtlas          every shadow view, depth
        SRV_ShadowViews    = 36,    // StructuredBuffer<ShadowView> shadowViews        matrix + atlas tile per view
    };
}