#pragma once
#include <DirectXMath.h>

namespace DXEngine {

    struct TransfomBufferData
    {
        DirectX::XMMATRIX WVP;
        DirectX::XMMATRIX Model;

    };
    struct MaterialBufferData
    {
        DirectX::XMFLOAT4 difColor;
        BOOL hasTexture;
        BOOL hasNormalMap;
        float pad[2];

    };
    struct PointLightData
    {
        PointLightData()
        {
            ZeroMemory(this, sizeof(PointLightData));
        }

        DirectX::XMFLOAT3 p_Position;
        float P_Range;
        DirectX::XMFLOAT4 p_Color;
        DirectX::XMFLOAT3 p_Attenuation;
        int p_Enabled;
    };
    struct SpotLightData
    {
        SpotLightData()
        {
            ZeroMemory(this, sizeof(SpotLightData));
        }
        DirectX::XMFLOAT4 s_Color;
        DirectX::XMFLOAT3 s_Position;
        float s_Range;
        DirectX::XMFLOAT3 s_Direction;
        float s_Cone;
        DirectX::XMFLOAT3 s_Attenuation;
        int s_Enabled;
    };

    struct DirectionalLightData
    {
        DirectionalLightData()
        {
            ZeroMemory(this, sizeof(DirectionalLightData));
        }
        DirectX::XMFLOAT4 d_Color;
        DirectX::XMFLOAT4 d_Ambient;
        DirectX::XMFLOAT4 d_Diffuse;
        DirectX::XMFLOAT3 d_Direction;
        int d_Enabled;

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
        CB_Material = 1,
        CB_Direction_Light = 2,
        CB_Point_Light = 3,
        CB_Spot_Light = 4,
        CB_UI = 5,
        CB_Bones = 6.

    };
}