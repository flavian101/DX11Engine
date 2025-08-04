#pragma once
#include <DirectXMath.h>

namespace DXEngine 
{
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

		DirectX::XMVECTOR DefaultForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		DirectX::XMVECTOR DefaultRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR camForward = DirectX::XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		DirectX::XMVECTOR camRight = DirectX::XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
		float moveLeftRight = 0.0f;
		float moveBackForward = 0.0f;

		float camYaw = 0.0f;
		float camPitch = 0.0f;

	private:
		DirectX::XMMATRIX camView;
		DirectX::XMMATRIX camProjection;
		DirectX::XMMATRIX camRotationMatrix;

		DirectX::XMVECTOR camPosition = DirectX::XMVectorSet(0.0f, 5.0f, -8.0f, 0.0f);
		DirectX::XMVECTOR camTarget = DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
		DirectX::XMVECTOR camUp = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);


	};

}