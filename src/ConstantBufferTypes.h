#pragma once
#include <DirectXMath.h>


using namespace DirectX;

struct cb_vsConstantBuffer
{
	XMMATRIX WVP;
	XMMATRIX Model;

};
struct Light
{
	Light()
	{
		ZeroMemory(this, sizeof(Light));
	}
    XMFLOAT3 pointPos;
    float pad;
    XMFLOAT3 spotPos;
    float range;
    XMFLOAT3 dir;
    float cone;
    XMFLOAT3 att;
    float pad2;
    XMFLOAT4 ambient;
    XMFLOAT4 diffuse;
};
struct cb_psConstantBuffer
{
	Light light;
};