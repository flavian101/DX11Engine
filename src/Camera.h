#pragma once
#include <DirectXMath.h>

using namespace DirectX;
class Camera
{
	
public:
	Camera(float viewWidth, float aspectRatio, float nearZ, float farZ);

	void UpdateCamera();

	const DirectX::XMMATRIX& GetView() const noexcept;
	const DirectX::XMVECTOR& GetPos() const noexcept;
	const DirectX::XMVECTOR& GetTarget() const noexcept;

	void SetProjection(DirectX::FXMMATRIX proj) noexcept;
	const DirectX::XMMATRIX& GetProjection() const noexcept;

	XMVECTOR DefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR DefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR camForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
	XMVECTOR camRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
	float moveLeftRight = 0.0f;
	float moveBackForward = 0.0f;

	float camYaw = 0.0f;
	float camPitch = 0.0f;

private:
	XMMATRIX camView;
	XMMATRIX camProjection;
	XMMATRIX camRotationMatrix;

	XMVECTOR camPosition = XMVectorSet(0.0f, 5.0f, -8.0f, 0.0f);
	XMVECTOR camTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR camUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);


};

