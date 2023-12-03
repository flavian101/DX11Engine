#pragma once
#include <DirectXMath.h>


using namespace DirectX;

struct cb_vsConstantBuffer
{
	XMMATRIX WVP;
	XMMATRIX World;

};
struct Light
{
	Light()
	{
		ZeroMemory(this, sizeof(Light));
	}
	XMFLOAT3 dir;
	float pad;
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
};
struct cb_psConstantBuffer
{
	Light light;
};