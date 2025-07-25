#include "Camera.h"

Camera::Camera(float viewWidth, float aspectRatio, float nearZ, float farZ)
{
	 camProjection = DirectX::XMMatrixPerspectiveLH(viewWidth, aspectRatio,nearZ, farZ);
	 camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);

}

void Camera::UpdateCamera()
{
	using namespace DirectX;
	camRotationMatrix = XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0);
	camTarget = XMVector3TransformCoord(DefaultForward, camRotationMatrix);
	camTarget = XMVector3Normalize(camTarget);

	XMMATRIX RotateYTempMatrix;
	RotateYTempMatrix = XMMatrixRotationY(camYaw);

	camRight = XMVector3TransformCoord(DefaultRight, RotateYTempMatrix);
	camUp = XMVector3TransformCoord(camUp, RotateYTempMatrix);
	camForward = XMVector3TransformCoord(DefaultForward, RotateYTempMatrix);

	camPosition += moveLeftRight * camRight;
	camPosition += moveBackForward * camForward;

	moveLeftRight = 0.0f;
	moveBackForward = 0.0f;

	camTarget = camPosition + camTarget;

	camView = XMMatrixLookAtLH(camPosition, camTarget, camUp);
}

const DirectX::XMMATRIX& Camera::GetView() const noexcept
{
	return camView;
}

const DirectX::XMVECTOR& Camera::GetPos()const noexcept
{
	return camPosition;
}

const DirectX::XMVECTOR& Camera::GetTarget() const noexcept
{
	return camTarget;
}

void Camera::SetProjection(DirectX::FXMMATRIX proj) noexcept
{
	camProjection = proj;
}

const DirectX::XMMATRIX& Camera::GetProjection() const noexcept
{
	return camProjection;
}


