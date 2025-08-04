#include "Model.h"


Model::Model(Graphics& gfx, std::shared_ptr<ShaderProgram> program)
{
	m_ModelTransform = std::make_shared<Transform>(gfx);
}

Model::~Model()
{
}

void Model::Update()
{
	
}

void Model::Render(Graphics& gfx)
{
	m_ModelTransform->Bind(gfx);
	m_Mesh->Draw(gfx);
}

const std::shared_ptr<MeshResource>& Model::GetMeshResource() const
{
	if(m_Mesh)
		return m_Mesh->GetResource();

	return nullptr;
}

HitInfo Model::TestRayIntersection(const Ray& ray)
{
	if (!m_Mesh || !IsPickable())
		return HitInfo();

	auto meshResource = GetMeshResource();
	if (!meshResource)
		return HitInfo();


	return RayIntersection::IntersectMesh(ray,
		meshResource,
		GetModelMatrix(),
		this);
}

const XMMATRIX& Model::GetModelMatrix() const
{
	if (!m_ModelTransform)
		return XMMatrixIdentity();

	return m_ModelTransform->GetTransform();
}

void Model::OnPicked()
{
	m_IsSelected = true;
}

void Model::OnUnpicked()
{
	m_IsSelected = false;

}


void Model::SetTranslation(const DirectX::XMFLOAT3& translation)
{
	m_ModelTransform->SetTranslation(translation);
}

const DirectX::XMVECTOR& Model::GetTranslation() const
{
	return m_ModelTransform->GetTranslation();
}

void Model::SetScale(const DirectX::XMFLOAT3& scale)
{
	m_ModelTransform->SetScale(scale);
}

const DirectX::XMVECTOR& Model::GetScale() const
{
	return m_ModelTransform->GetScale();
}

void Model::SetRotation(const DirectX::XMVECTOR& rotation)
{
	m_ModelTransform->SetRotation(rotation);
}

const DirectX::XMVECTOR& Model::GetRotation() const
{
	return m_ModelTransform->GetRotation();
}
