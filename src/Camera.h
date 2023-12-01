#pragma once
#include "Graphics.h"
#include <DirectXMath.h>
class Camera
{
public:
	Camera();


	DirectX::XMMATRIX GetView();


private:
	DirectX::XMMATRIX camView;
	DirectX::XMMATRIX camProjection;

	DirectX::XMVECTOR camPosition = DirectX::XMVectorSet(0.0f, 0.0f, -0.5f, 0.0f);
	DirectX::XMVECTOR camTarget = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	DirectX::XMVECTOR camUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
};

