#include "dxpch.h"
#include "Model.h"
#include "utils/mesh/Mesh.h"
#include "shaders/ShaderProgram.h"
#include "utils/material/Material.h"
#include <utils/Mesh/Utils/InputManager.h>
#include <algorithm>
#include "FrameTime.h"

namespace DXEngine {

	Model::Model()
	{
		m_Transform = std::make_shared<Transform>();
	}

	Model::Model(std::shared_ptr<Mesh> mesh)
		:Model()
	{
		SetMesh(mesh);
	}

	Model::~Model()
	{

	}

	void Model::Update(FrameTime deltatime)
	{
		// Update animation if skinned
		if (IsSkinned() && m_SkinningData->animationController)
		{
			UpdateAnimation(deltatime);
		}
	}
	bool Model::IsValid() const
	{
		return !m_Meshes.empty() && m_PrimaryMesh && m_PrimaryMesh->IsValid() && m_Transform;
	}

	//mesh management
	void Model::SetMesh(std::shared_ptr<Mesh> mesh)
	{
		if (!mesh)
			return;

		m_Meshes.clear();
		m_Meshes.emplace_back(mesh, "Primary");
		m_PrimaryMesh = mesh;

		InvalidateBounds();
		EnsureMeshMaterials(0);
		OnMeshChanged(0);
	}

	void Model::AddMesh(std::shared_ptr<Mesh> mesh, const std::string& name)
	{
		if (!mesh)
			return;
		size_t index = m_Meshes.size();
		m_Meshes.emplace_back(mesh, name.empty() ? "Mesh_" + std::to_string(index) : name);

		if (!m_PrimaryMesh)
		{
			m_PrimaryMesh = mesh;
		}
		InvalidateBounds();
		EnsureMeshMaterials(index);
		OnMeshChanged(index);
	}

	std::shared_ptr<Mesh> Model::GetMesh(size_t index)const
	{
		if (index >= m_Meshes.size())
			return nullptr;
		return m_Meshes[index].Mesh;
	}

	std::shared_ptr<Mesh> Model::GetMesh(const std::string& name) const
	{
		auto it = std::find_if(m_Meshes.begin(), m_Meshes.end(),
			[&name](const MeshEntry& entry) {return entry.Name == name; });

		return (it != m_Meshes.end()) ? it->Mesh : nullptr;
	}

	void Model::ClearMeshes()
	{
		m_Meshes.clear();
		m_PrimaryMesh.reset();
		InvalidateBounds();
	}

	//material 
	void Model::SetMaterial(std::shared_ptr<Material> material)
	{
		if (m_PrimaryMesh)
		{
			m_PrimaryMesh->SetMaterial(material);
			OnMaterialChanged(0, 0);
		}
	}

	void Model::SetMaterial(size_t meshIndex, size_t submeshIndex, std::shared_ptr<Material> material)
	{
		if (meshIndex >= m_Meshes.size())
			return;
		m_Meshes[meshIndex].Mesh->SetMaterial(submeshIndex, material);
		OnMaterialChanged(meshIndex, submeshIndex);
	}

	const std::shared_ptr<Material>& Model::GetMaterial(size_t submeshIndex) const
	{
		static std::shared_ptr<Material> nullMaterial;
		return m_PrimaryMesh ? m_PrimaryMesh->GetMaterial(submeshIndex): nullMaterial;
	}

	const std::shared_ptr<Material>& Model::GetMaterial(size_t meshIndex, size_t submeshIndex)const
	{
		static std::shared_ptr<Material> nullMaterial;
		if (meshIndex >= m_Meshes.size())
		{
			return nullMaterial;
		}
		return m_Meshes[meshIndex].Mesh->GetMaterial(submeshIndex);
	}

	bool Model::HasMaterial(size_t submeshIndex)const
	{
		return m_PrimaryMesh && m_PrimaryMesh->HasMaterial(submeshIndex);
	}

	void Model::EnsureDefaultMaterials()
	{
		for (size_t i = 0; i < m_Meshes.size(); i++)
		{
			EnsureMeshMaterials(i);
		}
	}

	void Model::SetTranslation(const DirectX::XMFLOAT3& translation)
	{
		if (m_Transform)
		{
			m_Transform->SetTranslation(translation);
			InvalidateBounds();
		}
	}

	const DirectX::XMVECTOR& Model::GetTranslation() const
	{
		static DirectX::XMVECTOR zero = DirectX::XMVectorZero();
		return m_Transform ? m_Transform->GetTranslation() : zero;
	}

	void Model::SetScale(const DirectX::XMFLOAT3& scale)
	{
		if (m_Transform)
		{
			m_Transform->SetScale(scale);
			InvalidateBounds();
		}
	}

	const DirectX::XMVECTOR& Model::GetScale() const
	{
		static DirectX::XMVECTOR one = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, 1.0f);
		return m_Transform ? m_Transform->GetScale() : one;
	}

	void Model::SetRotation(const DirectX::XMVECTOR& rotation)
	{
		if (m_Transform)
		{
			m_Transform->SetRotation(rotation);
			InvalidateBounds();
		}
	}

	void Model::SetRotation(float pitch, float yaw, float roll)
	{
		if (m_Transform)
		{
			m_Transform->SetRotation(pitch,yaw,roll);
			InvalidateBounds();
		}
	}

	const DirectX::XMVECTOR& Model::GetRotation() const
	{
		static DirectX::XMVECTOR identity = DirectX::XMQuaternionIdentity();
		return m_Transform ? m_Transform->GetRotation() : identity;
	}

	DirectX::XMMATRIX Model::GetModelMatrix() const
	{
		if (!m_Transform)
			return DirectX::XMMatrixIdentity();
		return m_Transform->GetTransform();
	}

	void Model::SetTransform(std::shared_ptr<Transform> transform)
	{
		if (m_Transform != transform)
		{
			m_Transform = transform;
			InvalidateBounds();
		}
	}

	const std::shared_ptr<Transform>& Model::GetTransform() const
	{
		static std::shared_ptr<Transform> nullTransform;

		if (m_Transform)
		{
			return m_Transform;
		}
		else
		{
			OutputDebugStringA("Warning: No Transform was found, returning null\n");
			return nullTransform;
		}

	}

	//Bounding Volume operations

	BoundingBox Model::GetLocalBoundingBox()const
	{
		if (m_BoundsDirty)
		{
			ComputeBounds();
		}
		return m_LocalBoundingBox;
	}

	BoundingSphere Model::GetLocalBoundingSphere()const
	{
		if (m_BoundsDirty) {
			ComputeBounds();
		}
		return m_LocalBoundingSphere;
	}

	BoundingBox Model::GetWorldBoundingBox()const
	{
		BoundingBox localBox = GetLocalBoundingBox();

		//Handle diffrent features
		if (IsInstanced() && m_InstanceData && !m_InstanceData->transforms.empty())
		{
			UpdateInstanceBounds();
			return m_LocalBoundingBox; //Already updated to world space
		}

		if (!m_Transform)
			return localBox;

		// Standard world space transform
		DirectX::XMMATRIX wordlMatrix = GetModelMatrix();
		DirectX::XMVECTOR corners[8];
		localBox.GetCorners(corners);

		BoundingBox worldBox;
		bool first = true;

		for (int i = 0; i < 8; i++)
		{
			DirectX::XMVECTOR worldCorner = DirectX::XMVector3Transform(corners[i], wordlMatrix);
			DirectX::XMFLOAT3 cornerPos;
			DirectX::XMStoreFloat3(&cornerPos, worldCorner);

			if (first)
			{
				worldBox =  BoundingBox(cornerPos, cornerPos);
				first = false;
			}
			else
			{
				worldBox.Expand(cornerPos);
			}

		}
		return worldBox;
	}

	BoundingSphere Model::GetWorldBoundingSphere()const
	{
		BoundingSphere localSphere = GetLocalBoundingSphere();

		if (!m_Transform)
			return localSphere;

		DirectX::XMMATRIX worldMatrix = GetModelMatrix();
		DirectX::XMVECTOR centerVec = DirectX::XMLoadFloat3(&localSphere.center);
		DirectX::XMVECTOR worldCenter = DirectX::XMVector3Transform(centerVec, worldMatrix);

		//calculate maximum scale to adjust radius
		DirectX::XMVECTOR scale = GetScale();
		float maxScale = DirectX::XMVectorGetX(DirectX::XMVectorMax(DirectX::XMVectorMax(
			DirectX::XMVectorSplatX(scale), DirectX::XMVectorSplatY(scale)), DirectX::XMVectorSplatZ(scale)));

		DirectX::XMFLOAT3 worldCenterFloat;
		DirectX::XMStoreFloat3(&worldCenterFloat, worldCenter);

		return BoundingSphere(worldCenterFloat, localSphere.radius * maxScale);
	}

	//Rendering Data access
	size_t Model::GetTotalSubmeshCount()const
	{
		size_t total = 0;
		for (const auto& entry : m_Meshes)
		{
			if (entry.Mesh)
			{
				total += std::max(size_t(1), entry.Mesh->GetSubmeshCount());
			}
		}
		return total;
	}

	uint32_t Model::GetTotalVertexCount() const
	{
		uint32_t total = 0;
		for (const auto& entry : m_Meshes)
		{
			if (entry.Mesh)
			{
				total += entry.Mesh->GetVertexCount();;
			}
		}
		return total;
	}

	size_t Model::GetTotalTriangleCount()const
	{
		size_t total = 0;
		for (const auto& entry : m_Meshes)
		{
			if (entry.Mesh)
			{
				if (entry.Mesh->GetSubmeshCount() > 1)
				{
					for (size_t i = 0; i < entry.Mesh->GetSubmeshCount(); ++i)
					{
						total += entry.Mesh->GetIndexCount()/3;
					}

				}
				else
				{
					total += entry.Mesh->GetIndexCount() / 3;

				}

			}
		}
		return total;
	}
	
	size_t Model::GetMemoryUsage() const
	{
		size_t usage = sizeof(*this);
		for (const auto& entry : m_Meshes)
		{
			if (entry.Mesh)
			{
				usage += entry.Mesh->GetTotalMemoryUsage();
			}
		}

		if (m_Transform) {
			usage = +sizeof(Transform);
		}
		return usage;
	}

	void Model::EnableInstancing()
	{
		if (!HasFeature(ModelFeature::Instanced))
		{
			AddFeature(ModelFeature::Instanced);
			m_InstanceData = std::make_unique<InstanceData>();
			InvalidateBounds();
		}
	}

	void Model::SetInstanceTransform(const std::vector<DirectX::XMFLOAT4X4>& transforms)
	{
		if (!m_InstanceData)
			EnableInstancing();

		m_InstanceData->SetInstances(transforms);
		InvalidateBounds();
	}

	void Model::AddInstance(const DirectX::XMFLOAT4X4& transform)
	{
		if (!m_InstanceData)
			EnableInstancing();

		m_InstanceData->AddInstance(transform);
		InvalidateBounds();
	}

	void Model::ClearInstances()
	{
		if (m_InstanceData)
		{
			m_InstanceData->ClearInstances();
			InvalidateBounds();
		}
	}

	size_t Model::GetInstanceCount() const
	{
		return m_InstanceData ? m_InstanceData->GetInstanceCount() : 0;
	}

	void Model::EnableSkinning(std::shared_ptr<Skeleton> skeleton)
	{
		if (!HasFeature(ModelFeature::Skinned))
		{
			AddFeature(ModelFeature::Skinned);
		}
		m_SkinningData = std::make_unique<SkinningData>();

		if (skeleton)
		{
			m_SkinningData->skeleton = skeleton;
			m_SkinningData->boneMatrices.resize(skeleton->GetBoneCount());
			// Initialize to identity matrices
			for (auto& mat : m_SkinningData->boneMatrices)
			{
				DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
			}
		}

		InvalidateBounds();
	}

	void Model::SetSkeleton(std::shared_ptr<Skeleton> skeleton)
	{
		if (!m_SkinningData)
		{
			EnableSkinning(skeleton);
		}
		else
		{
			m_SkinningData->skeleton = skeleton;
			if (skeleton)
			{
				m_SkinningData->boneMatrices.resize(skeleton->GetBoneCount());
				for (auto& mat : m_SkinningData->boneMatrices)
				{
					DirectX::XMStoreFloat4x4(&mat, DirectX::XMMatrixIdentity());
				}
			}
		}
		InvalidateBounds();
	}

	std::shared_ptr<Skeleton> Model::GetSkeleton() const
	{
		return m_SkinningData ? m_SkinningData->skeleton : nullptr;
	}

	void Model::SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices)
	{
		if (m_SkinningData)
		{
			m_SkinningData->UpdateBoneMatrices(matrices);
		}
	}

	const std::vector<DirectX::XMFLOAT4X4>& Model::GetBoneMatrices() const
	{
		static std::vector<DirectX::XMFLOAT4X4> empty;
		return m_SkinningData ? m_SkinningData->boneMatrices : empty;
	}

	void Model::SetAnimationController(std::shared_ptr<AnimationController> controller)
	{
		if (!m_SkinningData)
			EnableSkinning(nullptr);

		m_SkinningData->animationController = controller;

	}

	std::shared_ptr<AnimationController> Model::GetAnimationController() const
	{
		return m_SkinningData ? m_SkinningData->animationController : nullptr;
	}

	void Model::AddAnimationClip(std::shared_ptr<AnimationClip> clip)
	{
		if (!m_SkinningData)
		{
			OutputDebugStringA("Warning cannot add Clip");
			return;
		}
		m_SkinningData->AddAnimationClip(clip);
	}

	void Model::AddAnimationClip(const std::string& name, std::shared_ptr<AnimationClip> clip)
	{
		if (!m_SkinningData)
		{
			OutputDebugStringA("Warning: Cannot add animation to non-skinned model\n");
			return;
		}

		m_SkinningData->AddAnimationClip(name, clip);
	}

	std::shared_ptr<AnimationClip> Model::GetAnimationClip(const std::string& name) const
	{
		return m_SkinningData ? m_SkinningData->GetAnimationClip(name) : nullptr;
	}

	std::shared_ptr<AnimationClip> Model::GetAnimationClip(size_t index) const
	{
		return m_SkinningData ? m_SkinningData->GetAnimationClip(index) : nullptr;
	}

	size_t Model::GetAnimationClipCount() const
	{
		return m_SkinningData ? m_SkinningData->animationClips.size() : 0;
	}

	std::vector<std::string> Model::GetAnimationClipNames() const
	{
		return m_SkinningData->GetAnimationClipNames();
	}

	const std::vector<std::shared_ptr<AnimationClip>>& Model::GetAllAnimationClips() const
	{
		return m_SkinningData->GetAllAnimationClips();
	}

	void Model::PlayAnimation(const std::string& name, PlaybackMode mode)
	{
		m_SkinningData->PlayAnimation(name, mode);
	}

	void Model::PlayAnimation(size_t index, PlaybackMode mode)
	{
		m_SkinningData->PlayAnimation(index, mode);
	}

	void Model::PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode)
	{
		m_SkinningData->PlayAnimation(clip, mode);
	}

	void Model::StopAnimation()
	{
		m_SkinningData->StopAnimation();
	}

	void Model::PauseAnimation()
	{
		m_SkinningData->PauseAnimation();
	}

	void Model::ResumeAnimation()
	{
		m_SkinningData->ResumeAnimation();
	}


	bool Model::IsAnimating() const
	{
		return m_SkinningData &&
			m_SkinningData->animationController &&
			m_SkinningData->animationController->IsPlaying();
	}

	float Model::GetAnimationTime() const
	{
		return (m_SkinningData && m_SkinningData->animationController) ?
			m_SkinningData->animationController->GetCurrentTime() : 0.0f;
	}

	float Model::GetAnimationNormalizedTime() const
	{
		return (m_SkinningData && m_SkinningData->animationController) ?
			m_SkinningData->animationController->GetNormalizedTime() : 0.0f;
	}

	void Model::EnableLOD()
	{
		if (!HasFeature(ModelFeature::LOD))
		{
			AddFeature(ModelFeature::LOD);
			m_LODData = std::make_unique<LODData>();
		}
	}

	void Model::AddLODLevel(float distance, size_t meshIndex)
	{
		if (!m_LODData)
			EnableLOD();

		m_LODData->levels.push_back({ distance, meshIndex });

		// Sort by distance
		std::sort(m_LODData->levels.begin(), m_LODData->levels.end(),
			[](const LODData::LODLevel& a, const LODData::LODLevel& b) {
				return a.distance < b.distance;
			});
	}

	size_t Model::SelectLOD(float distance) const
	{
		return m_LODData ? m_LODData->SelectLOD(distance) : 0;
	}

	// ====== MORPH TARGETS FEATURE ======

	void Model::EnableMorphTargets()
	{
		if (!HasFeature(ModelFeature::Morph))
		{
			AddFeature(ModelFeature::Morph);
			m_MorphData = std::make_unique<MorphData>();
		}
	}

	//interfacePickable implementation
	HitInfo Model::TestRayIntersection(const Ray& ray)
	{
		if (!IsPickable() || m_Meshes.empty())
			return HitInfo();

		HitInfo closestHit;
		float closestDistance = FLT_MAX;

		for (size_t i = 0; i < m_Meshes.size(); i++)
		{
			const auto& mesh = m_Meshes[i].Mesh;
			if (!mesh || !mesh->IsValid())
				continue;

			auto meshResource = mesh->GetResource();
			if (!meshResource)
				continue;

			HitInfo hit = RayIntersection::IntersectMesh(ray, meshResource, GetModelMatrix(), this);
			if (hit.Hit && hit.Distance < closestDistance)
			{
				closestHit = hit;
				closestDistance = hit.Distance;
			}
		}

		return closestHit;
	}

	void Model::OnPicked()
	{
		m_IsSelected = true;
	}
	void Model::OnUnpicked()
	{
		m_IsSelected = false;
	}

	//virtual callbacks
	void Model::OnMeshChanged(size_t meshIndex)
	{
		InvalidateBounds();
	}

	void Model::OnMaterialChanged(size_t meshIndex, size_t submeshIndex)
	{

	}

	//internal helpers
	void Model::InvalidateBounds()
	{
		m_BoundsDirty = true;
	}

	void Model::ComputeBounds() const
	{
		if (m_Meshes.empty())
		{
			m_LocalBoundingBox = BoundingBox();
			m_LocalBoundingSphere = BoundingSphere();
			m_BoundsDirty = false;
			return;
		}

		bool first = true;
		BoundingBox combinedBox;

		for (const auto& entry : m_Meshes)
		{
			if (!entry.Mesh || !entry.Mesh->IsValid())
				continue;

			const BoundingBox& meshBox = entry.Mesh->GetBoundingBox();

			if (first)
			{
				combinedBox = meshBox;
				first = false;
			}
			else
			{
				combinedBox.Expand(meshBox);
			}
		}

		m_LocalBoundingBox = combinedBox;

		//compute bounding sphere from box
		DirectX::XMFLOAT3 center = combinedBox.GetCenter();
		DirectX::XMFLOAT3 extent = combinedBox.GetExtents();
		float radius = sqrtf(extent.x * extent.x + extent.y * extent.y + extent.z * extent.z);
		m_LocalBoundingSphere = BoundingSphere(center, radius);

		m_BoundsDirty = false;
	}

	void Model::EnsureMeshMaterials(size_t meshIndex)
	{
		if (meshIndex >= m_Meshes.size())
			return;
		auto& mesh = m_Meshes[meshIndex].Mesh;
		if (!mesh)
			return;

		//check if any submesh needs a default material
		size_t submeshCount = std::max(size_t(1), mesh->GetSubmeshCount());
		for (size_t i = 0; i < submeshCount; i++)
		{
			if (!mesh->HasMaterial(i))
			{
				auto defaultMaterial = MaterialFactory::CreateLitMaterial("Defaultmaterial");
				mesh->SetMaterial(i, defaultMaterial);
			}
		}
	}

	void Model::EnsureMaterialSlots()
	{
	}

	void Model::UpdateAnimation(FrameTime deltatime)
	{
		if (!m_SkinningData || !m_SkinningData->animationController)
		{
			return;
		}
		m_SkinningData->animationController->Update(deltatime);

		//Get updated bone matrices
		const auto& matrices = m_SkinningData->animationController->GetBoneMatrices();
		SetBoneMatrices(matrices);
	}

	void Model::UpdateInstanceBounds() const
	{
		if (!m_InstanceData || m_InstanceData->transforms.empty())
		{
			OutputDebugStringA("cannot update Instance Bounds");
			return;
		}

		BoundingBox localBox = GetLocalBoundingBox();
		BoundingBox combinedBox;
		bool first = true;

		for (const auto& instanceTransform : m_InstanceData->transforms)
		{
			DirectX::XMMATRIX instanceMatrix = DirectX::XMLoadFloat4x4(&instanceTransform);
			DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixMultiply(GetModelMatrix(), instanceMatrix);

			DirectX::XMVECTOR corners[8];
			localBox.GetCorners(corners);

			for (int i = 0; i < 8; i++)
			{
				DirectX::XMVECTOR worldCorner = DirectX::XMVector3Transform(corners[i], worldMatrix);
				DirectX::XMFLOAT3 cornerPos;
				DirectX::XMStoreFloat3(&cornerPos, worldCorner);

				if (first)
				{
					combinedBox = BoundingBox(cornerPos, cornerPos);
					first = false;
				}
				else
				{
					combinedBox.Expand(cornerPos);
				}
			}
		}
		m_LocalBoundingBox = combinedBox;
	}

	void Model::UpdateSkinnedBounds() const
	{
	}

	//factory methods 
	std::shared_ptr<Model> Model::CreateQuad(float width, float height)
	{
		auto mesh = Mesh::CreateQuad(width, height);
		auto model = std::make_shared<Model>(mesh);
		model->EnsureDefaultMaterials();
		return model;
	}

	std::shared_ptr<Model> Model::CreateCube(float size)
	{
		auto mesh = Mesh::CreateCube(size);
		auto model = std::make_shared<Model>(mesh);
		model->EnsureDefaultMaterials();
		return model;
	}

	std::shared_ptr<Model> Model::CreateSphere(float radius, uint32_t segments)
	{
		auto mesh = Mesh::CreateSphere(radius, segments);
		auto model = std::make_shared<Model>(mesh);
		model->EnsureDefaultMaterials();
		return model;
	}

	std::shared_ptr<Model> Model::CreatePlane(float width, float depth, uint32_t widthSegments, uint32_t depthSegments)
	{
		auto mesh = Mesh::CreatePlane(width, depth, widthSegments, depthSegments);
		auto model = std::make_shared<Model>(mesh);
		model->EnsureDefaultMaterials();
		return model;
	}

	std::shared_ptr<Model> Model::CreateInstancedModel(std::shared_ptr<Mesh> mesh)
	{
		auto model = std::make_shared<Model>(mesh);
		model->EnableInstancing();
		return model;
	}

	std::shared_ptr<Model> Model::CreateSkinnedModel(std::shared_ptr<Mesh> mesh, std::shared_ptr<Skeleton> skeleton)
	{
		auto model = std::make_shared<Model>(mesh);
		model->EnableSkinning(skeleton);
		return model;
	}
	std::string Model::GetDebugInfo()const
	{
		std::ostringstream oss;
		oss << "Model Debug Info:\n";
		oss << "Valid: " << (IsValid() ? "Yes" : "No") << "\n";
		oss << "Visible: " << (m_Visible ? "Yes" : "No") << "\n";

		// Feature info
		oss << "\nFeatures:\n";
		if (HasFeature(ModelFeature::Static)) oss << "  - Static\n";
		if (HasFeature(ModelFeature::Instanced))
		{
			oss << "  - Instanced (" << GetInstanceCount() << " instances)\n";
		}
		if (HasFeature(ModelFeature::Skinned))
		{
			oss << "  - Skinned";
			if (auto skeleton = GetSkeleton())
			{
				oss << " (" << skeleton->GetBoneCount() << " bones)";
			}
			oss << "\n";
		}
		if (HasFeature(ModelFeature::LOD))
		{
			oss << "  - LOD (" << (m_LODData ? m_LODData->levels.size() : 0) << " levels)\n";
		}
		if (HasFeature(ModelFeature::Morph))
		{
			oss << "  - Morph Targets\n";
		}

		// Standard info
		oss << "\nMesh Count: " << m_Meshes.size() << "\n";
		oss << "Total Submeshes: " << GetTotalSubmeshCount() << "\n";
		oss << "Total Triangles: " << GetTotalTriangleCount() << "\n";
		oss << "Memory Usage: " << GetMemoryUsage() << " bytes\n";

		return oss.str();
	}


	namespace ModelUtils
	{
		std::shared_ptr<Model> CombineModels(const std::vector<std::shared_ptr<Model>>& models)
		{
			if (models.empty())
				return nullptr;

			auto combinedModel = std::make_shared<Model>();

			for (const auto& model : models)
			{
				if (!model || !model->IsValid())
					continue;

				//add all meshes from this model
				for (size_t i = 0; i < model->GetMeshCount(); i++)
				{
					auto mesh = model->GetMesh(i);
					if (mesh)
					{
						std::string meshName = "combinedMesh_" + std::to_string(combinedModel->GetMeshCount());
						combinedModel->AddMesh(mesh, meshName);
					}
				}
			}
			combinedModel->EnsureDefaultMaterials();
			return combinedModel;
		}
		std::shared_ptr<Model> CreateFromMeshResource(std::shared_ptr<MeshResource> resource)
		{
			if (!resource || !resource->IsValid())
				return nullptr;

			auto mesh = std::make_shared<Mesh>(resource);
			auto model = std::make_shared<Model>(mesh);
			model->EnsureDefaultMaterials();

			return model;
		}
		std::shared_ptr<Model> CreateLODModel(const std::vector<std::shared_ptr<Mesh>>& lodMeshes)
		{
			if (lodMeshes.empty())
				return nullptr;
			auto model = std::make_shared<Model>();
			
			for (size_t i = 0; i < lodMeshes.size(); i++)
			{
				if (lodMeshes[i] && lodMeshes[i]->IsValid())
				{
					std::string lodName = "LOD_" + std::to_string(i);
					model->AddMesh(lodMeshes[i], lodName);
				}
			}

			model->EnsureDefaultMaterials();
			return model;
		}
	}
	}