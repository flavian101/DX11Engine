#include "Camera.h"
#include <algorithm>

namespace DXEngine {

	Camera::Camera(float viewWidth, float aspectRatio, float nearZ, float farZ)
	{
		camProjection = DirectX::XMMatrixPerspectiveLH(viewWidth, aspectRatio, nearZ, farZ);
		camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);

	}

	void Camera::UpdateCamera()
	{
		using namespace DirectX;

		camRotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(camPitch, camYaw, 0);
		camTarget = DirectX::XMVector3TransformCoord(DefaultForward, camRotationMatrix);
		camTarget = DirectX::XMVector3Normalize(camTarget);

		DirectX::XMMATRIX RotateYTempMatrix;
		RotateYTempMatrix = DirectX::XMMatrixRotationY(camYaw);

		camRight = DirectX::XMVector3TransformCoord(DefaultRight, RotateYTempMatrix);
		camUp = DirectX::XMVector3TransformCoord(camUp, RotateYTempMatrix);
		camForward = DirectX::XMVector3TransformCoord(DefaultForward, RotateYTempMatrix);

		camPosition += moveLeftRight * camRight;
		camPosition += moveBackForward * camForward;

		moveLeftRight = 0.0f;
		moveBackForward = 0.0f;

		camTarget = camPosition + camTarget;

		camView = DirectX::XMMatrixLookAtLH(camPosition, camTarget, camUp);
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

	void Camera::SetProjection(const DirectX::FXMMATRIX& proj) noexcept
	{
		camProjection = proj;
	}

	const DirectX::XMMATRIX& Camera::GetProjection() const noexcept
	{
		return camProjection;
	}


}