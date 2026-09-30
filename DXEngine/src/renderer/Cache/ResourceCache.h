#pragma once
#include "RHI/GraphicsDevice.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <functional>

namespace DXEngine {
	//Uniquely identifies a pipeline state. Hashed for cache lookup
	struct PipelineKey
	{
		size_t vertexLayoutHash = 0;
		MaterialType materialType = MaterialType::Lit;
		RHI::CullMode cullMode = RHI::CullMode::Back;
		RHI::BlendMode blendMode = RHI::BlendMode::Opaque;
		RHI::DepthTestMode depthTest = RHI::DepthTestMode::LessEqual;
		bool depthWrite = true;
		bool wireframe = false;
		bool skinned = false;
		bool instanced = false;

		bool operator==(const PipelineKey& o) const
		{
			return vertexLayoutHash == o.vertexLayoutHash
				&& materialType == o.materialType
				&& cullMode == o.cullMode
				&& blendMode == o.blendMode
				&& depthTest == o.depthTest
				&& depthWrite == o.depthWrite
				&& wireframe == o.wireframe
				&& skinned == o.skinned
				&& instanced == o.instanced;
		}
	};

	struct PipelineKeyHash {
		size_t operator()(const PipelineKey& k) const noexcept
		{
			// FNV-1a style combine
			size_t h = k.vertexLayoutHash;
			auto combine = [&](size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
			combine(std::hash<int>{}(static_cast<int>(k.materialType)));
			combine(std::hash<int>{}(static_cast<int>(k.cullMode)));
			combine(std::hash<int>{}(static_cast<int>(k.blendMode)));
			combine(std::hash<int>{}(static_cast<int>(k.depthTest)));
			combine(std::hash<bool>{}(k.depthWrite));
			combine(std::hash<bool>{}(k.wireframe));
			combine(std::hash<bool>{}(k.skinned));
			combine(std::hash<bool>{}(k.instanced));
			return h;
		}
	};

	//Resource cache
	//Owns all GPU_side cached resources. Everything goes through here.
	//No pass or sytems creates raw RHI resources directly
	class ResourceCache
	{
	public:
		explicit ResourceCache(std::shared_ptr<RHI::IGraphicsDevice> device);
		~ResourceCache() = default;

		bool Initialize();
		void Clear();

		//get or create a pipeline for the given key.
		//calls the factory only on cache miss - zero overhead on hit
		std::shared_ptr<RHI::IPipeline> GetOrCreatePipeline(
			const PipelineKey& key,
			std::function<std::shared_ptr<RHI::IPipeline>()> factory);

		//build a pipeline key from a material and mesh
		PipelineKey BuildPipelineKey(
			const Material* material,
			const Mesh* mesh,
			bool wireframe = false);

		//Invalidate all pipelines(e.g. after a wireframe toggle
		void InvalidatePipelines();


		//Enable or disable wireframe on all future pipelines
		void SetWireframeMode(bool enable);
		bool GetWireframeMode() const { return m_Wireframe; }

		//pool for reusable dynamic buffers, sized by usage pattern
		std::shared_ptr<RHI::IBuffer> GetOrCreateConstantBuffer(
			const std::string& name,
			uint32_t sizeBytes);


		//Transient buffer pool
		//per-frame buffers returned to the pool at EndFrame
		//used for instance data, bone matrices, etc
		std::shared_ptr<RHI::IBuffer> AcquireTransientBuffer(
			uint32_t sizeBytes,
			uint32_t stride,
			RHI::BufferType type);

		void ReturnTransientBuffers();// call at EndFrame


		//VertexLayout Hash
		size_t HashVertexLayout(const VertexLayout& layout) const;


		std::string GetDebugInfo() const;

		size_t GetPipelineCount()      const { return m_Pipelines.size(); }
		size_t GetConstantBufferCount()const { return m_ConstantBuffers.size(); }

	private:
		std::shared_ptr<RHI::IGraphicsDevice> m_Device;

		//pipeline cache
		std::unordered_map<PipelineKey, std::shared_ptr<RHI::IPipeline>, PipelineKeyHash> m_Pipelines;

		bool m_Wireframe;


		// Persistent constant buffers (keyed by name + size)
		struct CBEntry
		{
			std::shared_ptr<RHI::IBuffer> buffer;
			uint32_t sizeBytes = 0;
		};
		std::unordered_map<std::string, CBEntry> m_ConstantBuffers;

		// Transient buffer pool
		struct PooledBuffer
		{
			std::shared_ptr<RHI::IBuffer> buffer;
			uint32_t sizeBytes = 0;
			uint32_t stride = 0;
			RHI::BufferType type;
			bool inUse = false;
		};
		std::vector<PooledBuffer> m_TransientPool;


	};
}

