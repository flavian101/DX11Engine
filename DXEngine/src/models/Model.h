#pragma once
#include "ModelFeatures.h"
#include "picking/InterfacePickable.h"
#include "utils/Transform.h"
#include <memory>
#include <optional>
#include <DirectXMath.h>
#include "utils/mesh/Resource/MeshResource.h"

namespace DXEngine {

	class Mesh;
	class Material;

	class Model : public InterfacePickable
	{
	public:
	public:
		Model();
		explicit Model(std::shared_ptr<Mesh> mesh);
		virtual ~Model();

		virtual void Update(float deltaTime = 0.0f);
		bool IsValid()const;
		std::string GetDebugInfo()const;

		//feature Managment
		ModelFeature GetFeatures()const { return m_Features; }
		bool HasFeature(ModelFeature feature)const { return DXEngine::HasFeature(m_Features, feature); }
		void AddFeature(ModelFeature feature) { m_Features = m_Features | feature; }
		void RemoveFeature(ModelFeature feature)
		{
			m_Features = static_cast<ModelFeature>(
				static_cast<uint32_t>(m_Features) & ~static_cast<uint32_t>(feature)
				);
		}

		//mesh managment
		void SetMesh(std::shared_ptr<Mesh> mesh);//primary
		const std::shared_ptr<Mesh>& GetMesh()const { return m_PrimaryMesh; }
		void AddMesh(std::shared_ptr<Mesh> mesh, const std::string& name = "");// secondary meshes
		std::shared_ptr<Mesh> GetMesh(size_t index)const;
		std::shared_ptr<Mesh> GetMesh(const std::string& name)const;
		size_t GetMeshCount()const { return m_Meshes.size(); }
		void ClearMeshes();

		//material
		void SetMaterial(std::shared_ptr<Material> material); //primary mesh
		void SetMaterial(size_t meshIndex, size_t subMeshIndex, std::shared_ptr<Material> material);
		const std::shared_ptr<Material>& GetMaterial(size_t submeshIndex = 0)const;
		const std::shared_ptr<Material>& GetMaterial(size_t meshIndex,size_t submeshIndex = 0) const;
		bool HasMaterial(size_t submeshIndex = 0)const;
		void EnsureDefaultMaterials();


		// Transform operations
		void SetTranslation(const DirectX::XMFLOAT3& translation);
		const DirectX::XMVECTOR& GetTranslation() const;
		void SetScale(const DirectX::XMFLOAT3& scale);
		const DirectX::XMVECTOR& GetScale() const;
		void SetRotation(const DirectX::XMVECTOR& rotation);
		const DirectX::XMVECTOR& GetRotation() const;
		DirectX::XMMATRIX GetModelMatrix() const override;

		// Transform management
		void SetTransform(std::shared_ptr<Transform> transform);
		const std::shared_ptr<Transform>& GetTransform() const;

		//bounding volume operations
		BoundingBox GetLocalBoundingBox() const;
		BoundingSphere GetLocalBoundingSphere() const;
		virtual BoundingBox GetWorldBoundingBox()const;
		virtual BoundingSphere GetWorldBoundingSphere()const;

		//rendering data access
		size_t GetTotalSubmeshCount()const;
		uint32_t GetTotalVertexCount() const;
		size_t GetTotalTriangleCount()const;
		size_t GetMemoryUsage() const;

		bool IsVisible() const { return m_Visible; }
		void SetIsVisible(bool visible) { m_Visible = visible; }
		bool CastsShadows()const { return m_CastsShadows; }
		void setCastsShadows(bool casts) { m_CastsShadows = casts; }
		bool ReceivesShadows()const { return m_ReceivesShadows; }
		void SetReceivesShadows(bool receives){ m_ReceivesShadows = receives; }

		//picking
		bool IsSelected()const { return m_IsSelected; }
		void SetSelected(bool select){ m_IsSelected = select; }

					// ====== INSTANCING FEATURE ======
		// Check if model supports instancing
		bool IsInstanced() const { return HasFeature(ModelFeature::Instanced); }
		void EnableInstancing();
		InstanceData* GetInstanceData() { return m_InstanceData.get(); } //return nullptr if not
		const InstanceData* GetInstanceData() const { return m_InstanceData.get(); }
		//instance managment (automaticaly enables instancing)
		void SetInstanceTransform(const std::vector<DirectX::XMFLOAT4X4>& transforms);
		void AddInstance(const DirectX::XMFLOAT4X4& transform);
		void ClearInstances();
		size_t GetInstanceCount()const;

					// ====== SKINNING FEATURE ======
		 // Check if model has skinning
		bool IsSkinned()const { return HasFeature(ModelFeature::Skinned); }
		void EnableSkinning(std::shared_ptr<Skeleton> skeleton);
		// Get skinning data (nullptr if not skinned)
		SkinningData* GetSkinningData() { return m_SkinningData.get(); }
		const SkinningData* GetSkinningData() const { return m_SkinningData.get(); }
		// Skinning management
		void SetSkeleton(std::shared_ptr<Skeleton> skeleton);
		std::shared_ptr<Skeleton> GetSkeleton() const;
		void SetBoneMatrices(const std::vector<DirectX::XMFLOAT4X4>& matrices);
		const std::vector<DirectX::XMFLOAT4X4>& GetBoneMatrices() const;

		// Animation
		void SetAnimationController(std::shared_ptr<AnimationController> controller);
		std::shared_ptr<AnimationController> GetAnimationController() const;
		void AddAnimationClip(std::shared_ptr<AnimationClip> clip);
		void AddAnimationClip(const std::string& name, std::shared_ptr<AnimationClip> clip);
		std::shared_ptr<AnimationClip> GetAnimationClip(const std::string& name) const;
		std::shared_ptr<AnimationClip> GetAnimationClip(size_t index) const;
		size_t GetAnimationClipCount() const;
		std::vector<std::string> GetAnimationClipNames() const;
		const std::vector<std::shared_ptr<AnimationClip>>& GetAllAnimationClips() const;

		// Animation Playback
		void PlayAnimation(const std::string& name, PlaybackMode mode = PlaybackMode::Loop);
		void PlayAnimation(size_t index, PlaybackMode mode = PlaybackMode::Loop);
		void PlayAnimation(std::shared_ptr<AnimationClip> clip, PlaybackMode mode = PlaybackMode::Loop);
		void StopAnimation();
		void PauseAnimation();
		void ResumeAnimation();
		bool IsAnimating() const;
		float GetAnimationTime() const;
		float GetAnimationNormalizedTime() const;

					// ====== LOD FEATURE ======
		bool HasLOD() const { return HasFeature(ModelFeature::LOD); }
		void EnableLOD();
		LODData* GetLODData() { return m_LODData.get(); }
		const LODData* GetLODData() const { return m_LODData.get(); }
		void AddLODLevel(float distance, size_t meshIndex);
		size_t SelectLOD(float distance) const;

					// ====== MORPH TARGETS FEATURE ======
		bool HasMorphTargets() const { return HasFeature(ModelFeature::Morph); }
		void EnableMorphTargets();
		MorphData* GetMorphData() { return m_MorphData.get(); }
		const MorphData* GetMorphData() const { return m_MorphData.get(); }

		//factory Methods
		static std::shared_ptr<Model> CreateQuad(float width = 1.0f, float height = 1.0f);
		static std::shared_ptr<Model> CreateCube(float size = 1.0f);
		static std::shared_ptr<Model> CreateSphere(float radius = 1.0f, uint32_t segments = 32);
		static std::shared_ptr<Model> CreatePlane(float width = 10.0f, float depth = 10.0f,
			uint32_t widthSegments = 10, uint32_t depthSegments = 10);

		// Factory methods for specialized models
		static std::shared_ptr<Model> CreateInstancedModel(std::shared_ptr<Mesh> mesh);
		static std::shared_ptr<Model> CreateSkinnedModel(std::shared_ptr<Mesh> mesh,
			std::shared_ptr<Skeleton> skeleton);

	protected:
		HitInfo TestRayIntersection(const Ray& ray) override;
		void OnPicked() override;
		void OnUnpicked() override;
		void OnMeshChanged(size_t meshIndex);
		void OnMaterialChanged(size_t meshIndex, size_t submeshIndex);
		void InvalidateBounds();

	private:

		void ComputeBounds()const;
		void EnsureMeshMaterials(size_t meshIndex);
		void EnsureMaterialSlots();
		void UpdateAnimation(float deltaTime);
		void UpdateInstanceBounds() const;
		void UpdateSkinnedBounds() const;


		//mesh Storage
		struct MeshEntry
		{
			std::shared_ptr<Mesh> Mesh;
			std::string Name;

			MeshEntry(std::shared_ptr<DXEngine::Mesh> m, const std::string& name)
				:Mesh(m),Name(name)
			{}
		};

		private:

		std::vector<MeshEntry> m_Meshes;
		std::shared_ptr<Mesh> m_PrimaryMesh;
		std::shared_ptr<Transform> m_Transform;

		//cache Bounds ( mutable for lazy computations)
		mutable BoundingBox m_LocalBoundingBox;
		mutable BoundingSphere m_LocalBoundingSphere;
		mutable bool m_BoundsDirty = true;

		//render State
		bool m_Visible = true;
		bool m_CastsShadows = true;
		bool m_ReceivesShadows = true;
		bool m_IsSelected = false;

		ModelFeature m_Features;

		//optional fetures based on flags
		std::unique_ptr<InstanceData> m_InstanceData;
		std::unique_ptr<SkinningData> m_SkinningData;
		std::unique_ptr<LODData> m_LODData;
		std::unique_ptr<MorphData> m_MorphData;

	};

	namespace ModelUtils
	{
		std::shared_ptr<Model> CombineModels(const std::vector<std::shared_ptr<Model>>& models);
		std::shared_ptr<Model> CreateFromMeshResource(std::shared_ptr<MeshResource> resource);
		std::shared_ptr<Model> CreateLODModel(const std::vector<std::shared_ptr<Mesh>>& lodMeshes);

	}

}