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
	public:
		Model();
		virtual ~Model();

		// Transform operations
		void SetTranslation(const DirectX::XMFLOAT3& translation);
		const DirectX::XMVECTOR& GetTranslation() const;
		void SetScale(const DirectX::XMFLOAT3& scale);
		const DirectX::XMVECTOR& GetScale() const;
		void SetRotation(const DirectX::XMVECTOR& rotation);
		const DirectX::XMVECTOR& GetRotation() const;
		DirectX::XMMATRIX GetModelMatrix() const override;

		// Transform management
		void SetTransform(const std::shared_ptr<Transform>& transform);
		const std::shared_ptr<Transform>& GetTransform() const;

		// Mesh management
		const std::shared_ptr<Mesh>& GetMesh() const;
		void SetMesh(const std::shared_ptr<Mesh>& mesh);

		// Material management (higher level interface)
		void SetMaterial(std::shared_ptr<Material> material);
		void SetMaterial(size_t submeshIndex, std::shared_ptr<Material> material);
		const std::shared_ptr<Material>& GetMaterial(size_t submeshIndex = 0) const;
		bool HasMaterial(size_t submeshIndex = 0) const;
		void EnsureDefaultMaterial();

		// Rendering interface (higher level than mesh)
		void Render(const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;
		void RenderSubmesh(size_t submeshIndex, const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;
		void RenderInstanced(uint32_t instanceCount, const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;

		// Lower-level rendering control for the renderer
		void BindForRendering(const void* shaderByteCode = nullptr, size_t byteCodeLength = 0) const;
		void DrawAll() const;
		void Draw(size_t submeshIndex = 0) const;
		void DrawInstanced(uint32_t instanceCount, size_t submeshIndex = 0) const;

		// Mesh information (encapsulated)
		uint32_t GetIndexCount(size_t submeshIndex = 0) const;
		uint32_t GetVertexCount() const;
		size_t GetTotalTriangleCount() const;

		// Properties
		bool IsValid() const;
		bool IsSelected() const { return m_IsSelected; }
		size_t GetSubmeshCount() const;

		// Bounding information
		const BoundingBox& GetBoundingBox() const;           // Local space
		const BoundingSphere& GetBoundingSphere() const;     // Local space  
		BoundingBox GetWorldBoundingBox() const;             // World space

		// Update loop
		virtual void Update();

		// Debug and utility
		std::string GetDebugInfo() const;
		size_t GetMemoryUsage() const;

		// Factory methods for common shapes
		static std::shared_ptr<Model> CreateQuad(float width = 1.0f, float height = 1.0f);
		static std::shared_ptr<Model> CreateCube(float size = 1.0f);
		static std::shared_ptr<Model> CreateSphere(float radius = 1.0f, uint32_t segments = 32);
		static std::shared_ptr<Model> CreatePlane(float width = 10.0f, float depth = 10.0f,
			uint32_t widthSegments = 10, uint32_t depthSegments = 10);

	protected:
		// InterfacePickable implementation
		HitInfo TestRayIntersection(const Ray& ray) override;
		void OnPicked() override;
		void OnUnpicked() override;

		// Virtual callbacks for derived classes
		virtual void OnMeshChanged();

	protected:
		std::shared_ptr<Mesh> m_Mesh;
		std::shared_ptr<Transform> m_ModelTransform;
		bool m_IsSelected;

	};

}