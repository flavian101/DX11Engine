#include "dxpch.h"
#include "MovementBehavior.h"
#include "FrameTime.h"
#include "Camera.h"
#include <Core/Input.h>


namespace DXEngine
{
	MovementBehavior::MovementBehavior(float moveSpeed)
		:CameraBehavior(1.0f)
		,m_MoveSpeed(moveSpeed)

	{
	}
	CameraContribution MovementBehavior::GetCameraContribution(Camera& camera, FrameTime deltatime)
	{
		if (!IsActive())
			return CameraContribution();

		//frame distance 
		float frameDistance = m_MoveSpeed * deltatime;

		DirectX::XMFLOAT3 forward = camera.GetForwardVector();
		DirectX::XMFLOAT3 right = camera.GetRightVector();
		DirectX::XMFLOAT3 up = camera.GetUpVector();

		//calculate total movement diretion by combining all pressed keys
		DirectX::XMFLOAT3 moveDirection(0.0f, 0.0f, 0.0f);

		
		if (Input::IsKeyPressed('W'))
		{
			moveDirection.x += forward.x;
			moveDirection.y += forward.y;
			moveDirection.z += forward.z;
		}
		if (Input::IsKeyPressed('S'))
		{
			moveDirection.x -= forward.x;
			moveDirection.y -= forward.y;
			moveDirection.z -= forward.z;
		}
		if (Input::IsKeyPressed('A'))
		{
			moveDirection.x += right.x;
			moveDirection.y += right.y;
			moveDirection.z += right.z;
		}
		if (Input::IsKeyPressed('D'))
		{
			moveDirection.x -= right.x;
			moveDirection.y -= right.y;
			moveDirection.z -= right.z;
		}

		if (Input::IsKeyPressed(VK_SPACE))
		{
			moveDirection.x += up.x;
			moveDirection.y += up.y;
			moveDirection.z += up.z;
		}

		if (Input::IsKeyPressed(VK_LCONTROL))
		{
			moveDirection.x -= up.x;
			moveDirection.y -= up.y;
			moveDirection.z -= up.z;
		}
		//normalize movment direction to prevent faster diagnol movement
		DirectX::XMVECTOR movDirVector = DirectX::XMLoadFloat3(&moveDirection);
		movDirVector = DirectX::XMVector3Normalize(movDirVector);
		DirectX::XMStoreFloat3(&moveDirection, movDirVector);

		DirectX::XMFLOAT3 positionChange(
			moveDirection.x * frameDistance,
			moveDirection.y * frameDistance,
			moveDirection.z * frameDistance);

		return CameraContribution::Position(positionChange, GetPriority());
	}
}