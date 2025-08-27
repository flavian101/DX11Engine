#pragma once
#include <DirectXMath.h>
#include <memory>
#include <vector>

namespace DXEngine 
{
	class CameraBehavior;
	class FrameTime;

	class Camera
	{
	public:
		Camera();
		~Camera() = default;

		void SetProjectionParams(float fov, float aspect, float nearPlane, float farPlane);

		void Update(FrameTime deltatime);
		void UpdateViewMatrix();
		void UpdateProjectionMatrix();

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


		std::vector<std::shared_ptr<CameraBehavior>> m_Behaviors;

	};

}