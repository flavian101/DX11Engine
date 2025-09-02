#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

namespace DXEngine 
{
	class CameraBehavior;
	class FrameTime;

	struct CameraContribution
	{
		DirectX::XMFLOAT3 rotationChange;
		DirectX::XMFLOAT3 positionChange;
		float weight;

		CameraContribution(): rotationChange(0.0f,0.0f,0.0f), positionChange(0.0f, 0.0f, 0.0f), weight(0.0f)
		{}
		CameraContribution(const DirectX::XMFLOAT3& position,const DirectX::XMFLOAT3& rotation, float weight)
			:
			positionChange(position),
			rotationChange(rotation),
			weight(weight)
		{}

		static CameraContribution Rotation(const DirectX::XMFLOAT3 rotation, float weight)
		{
			return CameraContribution(DirectX::XMFLOAT3(0.0, 0.0f, 0.0f), rotation, weight);
		}
		static CameraContribution Position(const DirectX::XMFLOAT3 position, float weight)
		{
			return CameraContribution( position, DirectX::XMFLOAT3(0.0, 0.0f, 0.0f), weight);
		}

	};

	class Camera
	{
	public:
		Camera();
		~Camera() = default;

		void SetProjectionParams(float fov, float aspect, float nearPlane, float farPlane);

		void Update(FrameTime deltatime);
		void UpdateViewMatrix();
		void UpdateProjectionMatrix();
		void SetAspectRatio(float aspect);

		void AddBehaviour(std::shared_ptr<CameraBehavior> behavior);
		void RemoveBehaviour(std::shared_ptr<CameraBehavior> behavior);

		const DirectX::XMFLOAT4X4 GetViewMatrix();
		const DirectX::XMMATRIX& GetView() const noexcept;
		const DirectX::XMFLOAT4X4 GetProjectionMatrix()const { return m_ProjectionMatrix; }
		const DirectX::XMMATRIX& GetProjection() const noexcept;

		

		DirectX::XMFLOAT3 GetPosition() { return m_Position; };
		const DirectX::XMVECTOR& GetPos() const noexcept;
		DirectX::XMFLOAT3 GetRotation() { return m_Rotation; }
		void SetPosition(const DirectX::XMFLOAT3& position) { m_Position = position; m_ViewMatrixDirty = true; }
		void SetRotation(const DirectX::XMFLOAT3& rotation) { m_Rotation= rotation; m_ViewMatrixDirty = true;}
		

		DirectX::XMFLOAT3 GetForwardVector()const;
		DirectX::XMFLOAT3 GetRightVector() const;
		DirectX::XMFLOAT3 GetUpVector() const;
		void SetPitchLimit(float limitRadians) { m_PitchLimit = limitRadians; }
		float GetPithLimit() { return m_PitchLimit; }

	private:
		CameraContribution BlendRotationContribution(const std::vector<CameraContribution>& contribution);
	private:
		DirectX::XMFLOAT3 m_Position;
		DirectX::XMFLOAT3 m_Rotation;

		//cached matrices(alway updated when needed)
		DirectX::XMFLOAT4X4 m_ViewMatrix;
		DirectX::XMFLOAT4X4 m_ProjectionMatrix;
		bool m_ViewMatrixDirty;

		//camera props
		float m_FieldOfView;
		float m_AspectRatio;
		float m_NearPlane;
		float m_FarPlane;
		float m_PitchLimit;


		std::vector<std::shared_ptr<CameraBehavior>> m_Behaviors;

	};

}