#pragma once
#include <DirectXMath.h>


using namespace DirectX;

struct TransfomBufferData
{
	XMMATRIX WVP;
	XMMATRIX Model;

};
struct PointLightData
{
    PointLightData()
    {
        ZeroMemory(this, sizeof(PointLightData));
    }

    XMFLOAT3 p_Position;
    float P_Range;
    XMFLOAT4 p_Color;
    XMFLOAT3 p_Attenuation;
    int p_Enabled;
};
struct SpotLightData
{
    SpotLightData()
    {
        ZeroMemory(this, sizeof(SpotLightData));
    }
    XMFLOAT4 s_Color;
    XMFLOAT3 s_Position;
    float s_Range;
    XMFLOAT3 s_Direction;
    float s_Cone;
    XMFLOAT3 s_Attenuation;
    int s_Enabled;
};

struct DirectionalLightData
{
    DirectionalLightData()
    {
        ZeroMemory(this, sizeof(DirectionalLightData));
    }
    XMFLOAT4 d_Color;
    XMFLOAT4 d_Ambient;
    XMFLOAT4 d_Diffuse;
    XMFLOAT3 d_Direction;
    int d_Enabled;

};

enum BindSlot
{
    CB_Transform = 0,
    CB_Direction_Light = 1,
    CB_Point_Light = 2,
    CB_Spot_Light = 3,


};