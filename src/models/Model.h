#pragma once

#include "utils\Mesh.h"
#include "utils\Transform.h"

class Model
{
public:
	Model(Graphics& gfx, std::shared_ptr<ShaderProgram> program);

	~Model();

	void SetTranslation(const DirectX::XMFLOAT3& translation);
	const DirectX::XMVECTOR& GetTranslation() const;
	void SetScale(const DirectX::XMFLOAT3& scale);
	const DirectX::XMVECTOR& GetScale()const;
	void SetRotation(const DirectX::XMVECTOR& rotation);
	const DirectX::XMVECTOR& GetRotation()const;


	virtual void Update();
	virtual void Render(Graphics& gfx);

protected:
	std::shared_ptr<Mesh> m_Mesh;
	std::shared_ptr<Transform> m_ModelTransform;
};

