#include "dxpch.h"
#include "Model.h"
#include "utils/mesh/Mesh.h"
#include "shaders/ShaderProgram.h"
#include "utils/material/Material.h"
#include <utils/Mesh/InputManager.h>

namespace DXEngine {

	Model::Model()
		: m_IsSelected(false)
	{
		m_ModelTransform = std::make_shared<Transform>();
	}

	Model::~Model()
	{
	}

	void Model::Update()
	{
		// Override in derived classes for custom update logic
	}

	const std::shared_ptr<Mesh>& Model::GetMesh() const
	{
		return m_Mesh;
	}

	void Model::SetMesh(const std::shared_ptr<Mesh>& mesh)
	{
		if (m_Mesh != mesh)
		{
			m_Mesh = mesh;
			OnMeshChanged();
		}
	}

	void Model::SetMaterial(std::shared_ptr<Material> material)
	{
		if (m_Mesh)
		{
			m_Mesh->SetMaterial(material);
		}
	}

	void Model::SetMaterial(size_t submeshIndex, std::shared_ptr<Material> material)
	{
		if (m_Mesh)
		{
			m_Mesh->SetMaterial(submeshIndex, material);
		}
	}

	const std::shared_ptr<Material>& Model::GetMaterial(size_t submeshIndex) const
	{
		static std::shared_ptr<Material> nullMaterial;
		return m_Mesh ? m_Mesh->GetMaterial(submeshIndex) : nullMaterial;
	}

	bool Model::HasMaterial(size_t submeshIndex) const
	{
		return m_Mesh && m_Mesh->HasMaterial(submeshIndex);
	}

	void Model::EnsureDefaultMaterial()
	{
		if (!m_Mesh)
			return;

		// Check if any submesh needs a default material
		for (size_t i = 0; i < m_Mesh->GetSubmeshCount(); ++i)
		{
			if (!m_Mesh->HasMaterial(i))
			{
				auto defaultMaterial = MaterialFactory::CreateLitMaterial("DefaultModelMaterial");
				m_Mesh->SetMaterial(i, defaultMaterial);
			}
		}

		// If no submeshes but no material on main mesh
		if (m_Mesh->GetSubmeshCount() == 0 && !m_Mesh->HasMaterial(0))
		{
			auto defaultMaterial = MaterialFactory::CreateLitMaterial("DefaultModelMaterial");
			m_Mesh->SetMaterial(0, defaultMaterial);
		}
	}

	void Model::Render(const void* shaderByteCode, size_t byteCodeLength) const
	{
		if (!m_Mesh || !m_Mesh->IsValid())
			return;

		// Use InputLayoutManager for proper vertex layout binding
		if (shaderByteCode && byteCodeLength > 0)
		{
			auto layout = m_Mesh->GetVertexLayout();
			ScopedInputLayout scopedLayout(layout, shaderByteCode, byteCodeLength);

			if (!scopedLayout.IsValid())
			{
				OutputDebugStringA("Warning: Failed to create input layout for model rendering\n");
				return;
			}

			// Bind vertex buffers
			m_Mesh->BindBuffers();

			// Draw all submeshes or the main mesh
			if (m_Mesh->GetSubmeshCount() > 1)
			{
				m_Mesh->DrawAll();
			}
			else
			{
				m_Mesh->Draw(0);
			}
		}
		else
		{
			// Fallback to direct mesh binding
			m_Mesh->Bind();
			if (m_Mesh->GetSubmeshCount() > 1)
			{
				m_Mesh->DrawAll();
			}
			else
			{
				m_Mesh->Draw(0);
			}
		}
	}

	void Model::RenderSubmesh(size_t submeshIndex, const void* shaderByteCode, size_t byteCodeLength) const
	{
		if (!m_Mesh || !m_Mesh->IsValid())
			return;

		if (submeshIndex >= m_Mesh->GetSubmeshCount())
			return;

		if (shaderByteCode && byteCodeLength > 0)
		{
			auto layout = m_Mesh->GetVertexLayout();
			ScopedInputLayout scopedLayout(layout, shaderByteCode, byteCodeLength);

			if (!scopedLayout.IsValid())
				return;

			m_Mesh->BindBuffers();
			m_Mesh->Draw(submeshIndex);
		}
		else
		{
			m_Mesh->Bind();
			m_Mesh->Draw(submeshIndex);
		}
	}

	void Model::RenderInstanced(uint32_t instanceCount, const void* shaderByteCode, size_t byteCodeLength) const
	{
		if (!m_Mesh || !m_Mesh->IsValid() || instanceCount == 0)
			return;

		if (shaderByteCode && byteCodeLength > 0)
		{
			auto layout = m_Mesh->GetVertexLayout();
			ScopedInputLayout scopedLayout(layout, shaderByteCode, byteCodeLength);

			if (!scopedLayout.IsValid())
				return;

			m_Mesh->BindBuffers();
			m_Mesh->DrawInstanced(instanceCount, 0);
		}
		else
		{
			m_Mesh->Bind();
			m_Mesh->DrawInstanced(instanceCount, 0);
		}
	}

	bool Model::IsValid() const
	{
		return m_Mesh && m_Mesh->IsValid() && m_ModelTransform;
	}

	const BoundingBox& Model::GetBoundingBox() const
	{
		static BoundingBox emptyBox;
		return m_Mesh ? m_Mesh->GetBoundingBox() : emptyBox;
	}

	const BoundingSphere& Model::GetBoundingSphere() const
	{
		static BoundingSphere emptySphere;
		return m_Mesh ? m_Mesh->GetBoundingSphere() : emptySphere;
	}

	BoundingBox Model::GetWorldBoundingBox() const
	{
		if (!m_Mesh)
			return BoundingBox();

		BoundingBox localBox = m_Mesh->GetBoundingBox();
		DirectX::XMMATRIX worldMatrix = GetModelMatrix();

		// Transform bounding box to world space
		DirectX::XMVECTOR corners[8];
		localBox.GetCorners(corners);

		BoundingBox worldBox;
		bool first = true;

		for (int i = 0; i < 8; ++i)
		{
			DirectX::XMVECTOR worldCorner = DirectX::XMVector3Transform(corners[i], worldMatrix);
			DirectX::XMFLOAT3 cornerPos;
			DirectX::XMStoreFloat3(&cornerPos, worldCorner);

			if (first)
			{
				worldBox = BoundingBox(cornerPos, cornerPos);
				first = false;
			}
			else
			{
				worldBox.Expand(cornerPos);
			}
		}

		return worldBox;
	}

	size_t Model::GetSubmeshCount() const
	{
		return m_Mesh ? m_Mesh->GetSubmeshCount() : 0;
	}

	HitInfo Model::TestRayIntersection(const Ray& ray)
	{
		if (!m_Mesh || !IsPickable())
			return HitInfo();

		auto meshResource = m_Mesh->GetResource();
		if (!meshResource)
			return HitInfo();

		return RayIntersection::IntersectMesh(ray, meshResource, GetModelMatrix(), this);
	}

	DirectX::XMMATRIX Model::GetModelMatrix() const
	{
		if (!m_ModelTransform)
			return DirectX::XMMatrixIdentity();

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
		if (m_ModelTransform)
		{
			m_ModelTransform->SetTranslation(translation);
		}
	}

	const DirectX::XMVECTOR& Model::GetTranslation() const
	{
		static DirectX::XMVECTOR zero = DirectX::XMVectorZero();
		return m_ModelTransform ? m_ModelTransform->GetTranslation() : zero;
	}

	void Model::SetScale(const DirectX::XMFLOAT3& scale)
	{
		if (m_ModelTransform)
		{
			m_ModelTransform->SetScale(scale);
		}
	}

	const DirectX::XMVECTOR& Model::GetScale() const
	{
		static DirectX::XMVECTOR one = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		return m_ModelTransform ? m_ModelTransform->GetScale() : one;
	}

	void Model::SetRotation(const DirectX::XMVECTOR& rotation)
	{
		if (m_ModelTransform)
		{
			m_ModelTransform->SetRotation(rotation);
		}
	}

	const DirectX::XMVECTOR& Model::GetRotation() const
	{
		static DirectX::XMVECTOR identity = DirectX::XMQuaternionIdentity();
		return m_ModelTransform ? m_ModelTransform->GetRotation() : identity;
	}

	void Model::SetTransform(const std::shared_ptr<Transform>& transform)
	{
		if (m_ModelTransform != transform)
		{
			m_ModelTransform = transform;
		}
	}

	const std::shared_ptr<Transform>& Model::GetTransform() const
	{
		return m_ModelTransform;
	}

	std::string Model::GetDebugInfo() const
	{
		std::ostringstream oss;
		oss << "Model Debug Info:\n";
		oss << "Valid: " << (IsValid() ? "Yes" : "No") << "\n";
		oss << "Selected: " << (m_IsSelected ? "Yes" : "No") << "\n";
		oss << "Pickable: " << (IsPickable() ? "Yes" : "No") << "\n";

		if (m_Mesh)
		{
			oss << "Submeshes: " << m_Mesh->GetSubmeshCount() << "\n";
			oss << "Has Material: " << (HasMaterial() ? "Yes" : "No") << "\n";
			oss << m_Mesh->GetDebugInfo();
		}
		else
		{
			oss << "No mesh attached\n";
		}

		return oss.str();
	}

	// High-level rendering methods that encapsulate mesh operations
	void Model::BindForRendering(const void* shaderByteCode, size_t byteCodeLength) const
	{
		if (!m_Mesh || !m_Mesh->IsValid())
			return;

		if (shaderByteCode && byteCodeLength > 0)
		{
			auto layout = m_Mesh->GetVertexLayout();
			auto inputLayout = InputLayoutManager::Instance().GetInputLayout(layout, shaderByteCode, byteCodeLength);

			if (inputLayout)
			{
				RenderCommand::GetContext()->IASetInputLayout(inputLayout.Get());
			}
		}

		m_Mesh->BindBuffers();
	}

	void Model::DrawAll() const
	{
		if (!m_Mesh || !m_Mesh->IsValid())
			return;

		if (m_Mesh->GetSubmeshCount() > 1)
		{
			m_Mesh->DrawAll();
		}
		else
		{
			m_Mesh->Draw(0);
		}
	}

	void Model::Draw(size_t submeshIndex) const
	{
		if (!m_Mesh || !m_Mesh->IsValid())
			return;

		if (submeshIndex < m_Mesh->GetSubmeshCount())
		{
			m_Mesh->Draw(submeshIndex);
		}
	}

	void Model::DrawInstanced(uint32_t instanceCount, size_t submeshIndex) const
	{
		if (!m_Mesh || !m_Mesh->IsValid() || instanceCount == 0)
			return;

		if (submeshIndex < m_Mesh->GetSubmeshCount())
		{
			m_Mesh->DrawInstanced(instanceCount, submeshIndex);
		}
	}

	uint32_t Model::GetIndexCount(size_t submeshIndex) const
	{
		return m_Mesh ? m_Mesh->GetIndexCount(submeshIndex) : 0;
	}

	uint32_t Model::GetVertexCount() const
	{
		return m_Mesh ? m_Mesh->GetVertexCount() : 0;
	}

	size_t Model::GetTotalTriangleCount() const
	{
		if (!m_Mesh)
			return 0;

		size_t totalTriangles = 0;
		if (m_Mesh->GetSubmeshCount() > 1)
		{
			for (size_t i = 0; i < m_Mesh->GetSubmeshCount(); ++i)
			{
				totalTriangles += m_Mesh->GetIndexCount(i) / 3;
			}
		}
		else
		{
			totalTriangles = m_Mesh->GetIndexCount(0) / 3;
		}
		size_t Model::GetMemoryUsage() const
		{
			size_t usage = sizeof(*this);
			if (m_Mesh)
			{
				usage += m_Mesh->GetTotalMemoryUsage();
			}
			if (m_ModelTransform)
			{
				usage += sizeof(Transform);
			}
			return usage;
		}

		void Model::OnMeshChanged()
		{
			// Ensure the new mesh has materials if needed
			EnsureDefaultMaterial();

			// Override in derived classes for custom behavior
		}

		// Factory methods for common model types
		std::shared_ptr<Model> Model::CreateQuad(float width, float height)
		{
			auto model = std::make_shared<Model>();
			auto mesh = Mesh::CreateQuad(width, height);
			model->SetMesh(mesh);
			model->EnsureDefaultMaterial();
			return model;
		}

		std::shared_ptr<Model> Model::CreateCube(float size)
		{
			auto model = std::make_shared<Model>();
			auto mesh = Mesh::CreateCube(size);
			model->SetMesh(mesh);
			model->EnsureDefaultMaterial();
			return model;
		}

		std::shared_ptr<Model> Model::CreateSphere(float radius, uint32_t segments)
		{
			auto model = std::make_shared<Model>();
			auto mesh = Mesh::CreateSphere(radius, segments);
			model->SetMesh(mesh);
			model->EnsureDefaultMaterial();
			return model;
		}

		std::shared_ptr<Model> Model::CreatePlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments)
		{
			auto model = std::make_shared<Model>();
			auto mesh = Mesh::CreatePlane(width, depth, widthSegments, depthSegments);
			model->SetMesh(mesh);
			model->EnsureDefaultMaterial();
			return model;
		}

	}