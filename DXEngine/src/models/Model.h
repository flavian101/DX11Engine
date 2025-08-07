#pragma once
#include "picking/InterfacePickable.h"
#include "utils/Transform.h"
#include <memory>
#include <DirectXMath.h>

namespace DXEngine {
	class Mesh;
	class ShaderProgram;

	class Model : public InterfacePickable
	{
	public:
		Model(const std::shared_ptr<ShaderProgram>& program);

		~Model();

		void SetTranslation(const DirectX::XMFLOAT3& translation);
		const DirectX::XMVECTOR& GetTranslation() const;
		void SetScale(const DirectX::XMFLOAT3& scale);
		const DirectX::XMVECTOR& GetScale()const;
		void SetRotation(const DirectX::XMVECTOR& rotation);
		const DirectX::XMVECTOR& GetRotation()const;
		const DirectX::XMMATRIX& GetModelMatrix() const override;



		virtual void Update();

		bool IsSelected()const { return m_IsSelected; }
		const std::shared_ptr<Mesh>& GetMesh()const;



	private:
		bool m_IsSelected;
	protected:
		std::shared_ptr<Mesh> m_Mesh;
		std::shared_ptr<Transform> m_ModelTransform;



		// Inherited via InterfacePickable
		HitInfo TestRayIntersection(const Ray& ray)override;


		void OnPicked()override;

		void OnUnpicked() override;

	};

}