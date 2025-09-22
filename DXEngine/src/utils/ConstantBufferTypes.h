#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>
#include <memory>
#include <array>

namespace DXEngine {

    struct TransfomBufferData
    {
        DirectX::XMMATRIX WVP;
        DirectX::XMMATRIX Model;
        DirectX::XMFLOAT3 cameraPosition;
        float time;
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

    struct PointLightGPU
    {
        DirectX::XMFLOAT3 position;
        float radius;
        DirectX::XMFLOAT3 color;
        float intensity;
        DirectX::XMFLOAT3 attenuation; // constant, linear, quadratic
        float shadowMapIndex; // -1 if no shadows
    };

    struct SpotLightGPU
    {
        DirectX::XMFLOAT3 position;
        float range;
        DirectX::XMFLOAT3 direction;
        float innerCone;
        DirectX::XMFLOAT3 color;
        float outerCone;
        float intensity;
        DirectX::XMFLOAT3 attenuation;
        float shadowMapIndex;
        DirectX::XMMATRIX shadowMatrix;
    };

    // Unified light buffer for GPU
    struct SceneLightData
    {
        static constexpr uint32_t MAX_DIRECTIONAL_LIGHTS = 4;
        static constexpr uint32_t MAX_POINT_LIGHTS = 64;
        static constexpr uint32_t MAX_SPOT_LIGHTS = 32;

        // Light counts
        uint32_t directionalLightCount;
        uint32_t pointLightCount;
        uint32_t spotLightCount;
        uint32_t padding;

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
        std::array<PointLightGPU, MAX_POINT_LIGHTS> pointLights;
        std::array<SpotLightGPU, MAX_SPOT_LIGHTS> spotLights;
    };

    

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
        CB_Post_Process = 6        // For post-processing effects


    };
}