#pragma once
#include "picking/InterfacePickable.h"
#include "utils/Transform.h"
#include <memory>
#include <DirectXMath.h>
#include <utils/Mesh/MeshResource.h>

namespace DXEngine {

	class Mesh;
	class Material;
	struct RenderSubmission;
	class Renderer;

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
		bool setCastsShadows(bool casts) { m_CastsShadows = casts; }
	
		bool ReceivesShadows()const { return m_ReceivesShadows; }
		void SetReceivesShadows(bool receives){ m_ReceivesShadows = receives; }

		//picking
		bool IsSelected()const { return m_IsSelected; }
		void SetSelected(bool select){ m_IsSelected = select; }

		static std::shared_ptr<Model> CreateQuad(float width = 1.0f, float height = 1.0f);
		static std::shared_ptr<Model> CreateCube(float size = 1.0f);
		static std::shared_ptr<Model> CreateSphere(float radius = 1.0f, uint32_t segments = 32);
		static std::shared_ptr<Model> CreatePlane(float width = 10.0f, float depth = 10.0f,
			uint32_t widthSegments = 10, uint32_t depthSegments = 10);
	protected:

		HitInfo TestRayIntersection(const Ray& ray) override;
		void OnPicked() override;
		void OnUnpicked() override;

		//virtual callbacks
		virtual void OnMeshChanged(size_t meshIndex);
		virtual void OnMaterialChanged(size_t meshIndex, size_t submeshIndex);
	protected:
		void InvalidateBounds();

	private:

		void ComputeBounds()const;
		void EnsureMeshMaterials(size_t meshIndex);

		//mesh Storage
		struct MeshEntry
		{
			std::shared_ptr<Mesh> Mesh;
			std::string Name;

			MeshEntry(std::shared_ptr<DXEngine::Mesh> m, const std::string& name)
				:Mesh(m),Name(name)
			{}
		};

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
	};

	class InstanceModel :public Model
	{
	public:
		InstanceModel();
		explicit InstanceModel(std::shared_ptr<Mesh> mesh);

		void SetInstanceTransform(const std::vector<DirectX::XMFLOAT4X4>& transforms);
		void AddInstance(const DirectX::XMFLOAT4X4& transform);

		void ClearInstances();

		size_t GetInstanceCount()const { return m_InstanceTransforms.size(); }
		const std::vector<DirectX::XMFLOAT4X4>& GetInstanceTransforms() const { return m_InstanceTransforms; }

		BoundingBox GetWorldBoundingBox()const override;
		BoundingSphere GetWorldBoundingSphere()const override;

	private:
		std::vector<DirectX::XMFLOAT4X4> m_InstanceTransforms;

	};

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

	namespace ModelUtils
	{
		std::shared_ptr<Model> CombineModels(const std::vector<std::shared_ptr<Model>>& models);
		std::shared_ptr<Model> CreateFromMeshResource(std::shared_ptr<MeshResource> resource);
		std::shared_ptr<Model> CreateLODModel(const std::vector<std::shared_ptr<Mesh>>& lodMeshes);

	}

}