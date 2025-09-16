#pragma once
#include "Model.h"


namespace DXEngine
{
	class SkinnedModel : public Model
	{
	public:
		SkinnedModel();
		explicit SkinnedModel(std::shared_ptr<Mesh> mesh);

		void SetBoneMatrices(const std::vector< DirectX::XMFLOAT4X4>& boneMatrices);

		const std::vector< DirectX::XMFLOAT4X4>& GetBoneMatrices() const { return m_BoneMatrices; }

		void Update(float deltaTime) override;

	private:
		std::vector< DirectX::XMFLOAT4X4> m_BoneMatrices;

	};
}

