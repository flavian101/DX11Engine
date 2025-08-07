#pragma once

namespace DXEngine {

	class Transform
	{
	public:
		Transform();

		//getters and setters 
		void SetTranslation(const DirectX::XMFLOAT3& translation);
		const DirectX::XMVECTOR& GetTranslation() const;
		void SetScale(const DirectX::XMFLOAT3& scale);
		const DirectX::XMVECTOR& GetScale()const;
		void SetRotation(const DirectX::XMVECTOR& rotation);
		const DirectX::XMVECTOR& GetRotation()const;
		const DirectX::XMMATRIX& GetTransform() const;


		void Bind();

	private:
		DirectX::XMMATRIX m_WorldTransform;
		DirectX::XMVECTOR m_Scale;
		DirectX::XMVECTOR m_Translation;
		DirectX::XMVECTOR m_Rotation;


	};

}