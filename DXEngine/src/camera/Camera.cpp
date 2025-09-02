#include "dxpch.h"
#include "camera/Camera.h"
#include "FrameTime.h"
#include "CameraBehavior.h"


namespace DXEngine {

	Camera::Camera()
		: m_Position(-10.0f, 8.0f, 0.0f)
		, m_Rotation(0.0f, 0.0f, 0.0f)
		, m_ViewMatrixDirty(true)
		, m_FieldOfView(DirectX::XM_PIDIV4)//45 
		, m_AspectRatio((float)16.0f / (float)9.0f)
		, m_NearPlane(0.5f)
		, m_FarPlane(1000.0f)
		, m_PitchLimit(1.5f)
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
		std::vector<CameraContribution> contributions;

		for (auto& behavior : m_Behaviors)
		{
			if (behavior->IsActive())
			{
				CameraContribution contribution = behavior->GetCameraContribution(*this, deltatime);
				if (contribution.weight > 0.0f)
				{
					contributions.push_back(contribution);
				}
			}
		}

		CameraContribution finalChange = BlendRotationContribution(contributions);

		m_Position.x += finalChange.positionChange.x; 
		m_Position.y += finalChange.positionChange.y; 
		m_Position.z += finalChange.positionChange.z; 

		m_Rotation.x += finalChange.rotationChange.x;   // Pitch
		m_Rotation.y += finalChange.rotationChange.y;   // Yaw
		m_Rotation.z += finalChange.rotationChange.z;   // Roll

		//clamp pith to prevent over-rotation(looking too far up/down)
		m_Rotation.x = std::max(-m_PitchLimit, std::min(m_PitchLimit, m_Rotation.x));


		// Mark view matrix as needing update
		m_ViewMatrixDirty = true;


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

	void Camera::SetAspectRatio(float aspect)
	{
		m_AspectRatio = aspect;
		UpdateProjectionMatrix();
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

	CameraContribution Camera::BlendRotationContribution(const std::vector<CameraContribution>& contributions)
	{
		if (contributions.empty())
			return CameraContribution();

		CameraContribution blended;
		float totalWeight = 0.0f;
		
		for (const auto& contribution : contributions)
		{
			//position
			blended.positionChange.x += contribution.positionChange.x * contribution.weight;
			blended.positionChange.y += contribution.positionChange.y * contribution.weight;
			blended.positionChange.z += contribution.positionChange.z * contribution.weight;

			//rotation
			blended.rotationChange.x += contribution.rotationChange.x * contribution.weight;
			blended.rotationChange.y += contribution.rotationChange.y * contribution.weight;
			blended.rotationChange.z += contribution.rotationChange.z * contribution.weight;
			totalWeight += contribution.weight;
		}

		//Normalize by total weight to get Weighted average 
		if (totalWeight > 0.0f)
		{
			blended.positionChange.x /= totalWeight;
			blended.positionChange.y /= totalWeight;
			blended.positionChange.z /= totalWeight;

			blended.rotationChange.x /= totalWeight;
			blended.rotationChange.y /= totalWeight;
			blended.rotationChange.z /= totalWeight;

			//final blended result has full weight
			blended.weight = 1.0f;

		}

		return blended;
	}

}