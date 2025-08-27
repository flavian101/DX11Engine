#include "dxpch.h"
#include "camera/Camera.h"
#include "FrameTime.h"
#include "CameraBehavior.h"


namespace DXEngine {

	Camera::Camera()
		: m_Position(0.0f, 20.0f, 0.0f)
		, m_Rotation(0.0f, 0.0f, 0.0f)
		, m_ViewMatrixDirty(true)
		, m_FieldOfView(DirectX::XM_PIDIV4)//45 
		, m_AspectRatio((float)16.0f / (float)9.0f)
		, m_NearPlane(0.5f)
		, m_FarPlane(1000.0f)
	{
		DirectX::XMStoreFloat4x4(&m_ViewMatrix, DirectX::XMMatrixIdentity());
		DirectX::XMStoreFloat4x4(&m_ProjectionMatrix, DirectX::XMMatrixIdentity());
		UpdateProjectionMatrix();
	}

	void Camera::SetProjectionParams(float fov, float aspect, float nearPlane, float farPlane)
	{
		m_FieldOfView = fov;
		m_AspectRatio = aspect;
		m_NearPlane = nearPlane;
		m_FarPlane = farPlane;
		UpdateProjectionMatrix();
	}

	void Camera::Update(FrameTime deltatime)
	{
		for (auto& behavior : m_Behaviors)
		{
			if (behavior->IsActive())
			{
				behavior->Update(*this, deltatime);
			}
		}
		if (m_ViewMatrixDirty)
		{
			UpdateViewMatrix();
			m_ViewMatrixDirty = false;
		}
	}

	void Camera::UpdateViewMatrix()
	{
		using namespace DirectX;

		DirectX::XMFLOAT3 f_forw= GetForwardVector();
		DirectX::XMVECTOR forward = DirectX::XMLoadFloat3(&f_forw);
		DirectX::XMFLOAT3 f_up = GetUpVector();
		DirectX::XMVECTOR up = DirectX::XMLoadFloat3(&f_up);
		DirectX::XMVECTOR position = DirectX::XMLoadFloat3(&m_Position);

		//view matrix
		DirectX::XMMATRIX view = DirectX::XMMatrixLookToLH(position, forward, up);
		DirectX::XMStoreFloat4x4(&m_ViewMatrix, view);
		
	}

	void Camera::UpdateProjectionMatrix()
	{
		DirectX::XMMATRIX projection = DirectX::XMMatrixPerspectiveFovLH(m_FieldOfView, m_AspectRatio, m_NearPlane, m_FarPlane);
		DirectX::XMStoreFloat4x4(&m_ProjectionMatrix, projection);
	}

	void Camera::AddBehaviour(std::shared_ptr<CameraBehavior> behavior)
	{
		m_Behaviors.push_back(behavior);
	}

	void Camera::RemoveBehaviour(std::shared_ptr<CameraBehavior> behavior)
	{
		m_Behaviors.erase(std::remove(m_Behaviors.begin(), m_Behaviors.end(), behavior), m_Behaviors.end());
	}

	const DirectX::XMFLOAT4X4 Camera::GetViewMatrix()
	{
		if (m_ViewMatrixDirty)
		{
			UpdateViewMatrix();
			m_ViewMatrixDirty = false;
		}
		return m_ViewMatrix;
	}

	const DirectX::XMMATRIX& Camera::GetView() const noexcept
	{
		DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&m_ViewMatrix);
		return view;
	}

	const DirectX::XMMATRIX& Camera::GetProjection() const noexcept
	{
		DirectX::XMMATRIX projection = DirectX::XMLoadFloat4x4(&m_ProjectionMatrix);
		return projection;
	}

	const DirectX::XMVECTOR& Camera::GetPos() const noexcept
	{
		DirectX::XMVECTOR pos = DirectX::XMLoadFloat3(&m_Position);
		return pos;
	}

	DirectX::XMFLOAT3 Camera::GetForwardVector() const
	{
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);//pitch, yaw roll
		DirectX::XMVECTOR forward = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 0.0f, -1.0f, 0.0f), rotationMatrix);
		DirectX::XMFLOAT3 result;
		DirectX::XMStoreFloat3(&result, forward);
		return result;
	}

	DirectX::XMFLOAT3 Camera::GetRightVector() const
	{
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		DirectX::XMVECTOR right = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f), rotationMatrix);

		DirectX::XMFLOAT3 result;
		DirectX::XMStoreFloat3(&result, right);
		return result;
	}

	DirectX::XMFLOAT3 Camera::GetUpVector() const
	{
		DirectX::XMMATRIX rotationMatrix = DirectX::XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
		DirectX::XMVECTOR right = DirectX::XMVector3TransformNormal(DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f), rotationMatrix);

		DirectX::XMFLOAT3 result;
		DirectX::XMStoreFloat3(&result, right);
		return result;
	}

}