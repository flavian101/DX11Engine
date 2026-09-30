#include "dxpch.h"
#include "ResourceCache.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <sstream>

namespace DXEngine {
	ResourceCache::ResourceCache(std::shared_ptr<RHI::IGraphicsDevice> device)
		:
		m_Device(device)
	{}

	bool ResourceCache::Initialize()
	{
		if (!m_Device)
		{
			OutputDebugStringA("ERROR: ResourceCache::Initialize - null device\n");
			return false;
		}

		//pre-warm with reasonable capacity estimates
		m_Pipelines.reserve(64);
		m_ConstantBuffers.reserve(16);
		m_TransientPool.reserve(32);

		OutputDebugStringA("ResourceCache initialized\n");


		return true;
	}

	void ResourceCache::Clear()
	{
		m_Pipelines.clear();
		m_ConstantBuffers.clear();
		m_TransientPool.clear();
		OutputDebugStringA("ResourceCache cleared\n");
	}

	std::shared_ptr<RHI::IPipeline> ResourceCache::GetOrCreatePipeline(const PipelineKey& key, std::function<std::shared_ptr<RHI::IPipeline>()> factory)
	{
		auto it = m_Pipelines.find(key);
		if (it != m_Pipelines.end())
		{
			return it->second; ///cache hit - zero cost
		}
		//cache miss - create and store
		auto pipeline = factory();
		if (!pipeline)
		{
			OutputDebugStringA("ERROR: ResourceCache::GetOrCreatePipeline - factory returned null\n");
			return nullptr;
		}
		m_Pipelines.emplace(key, pipeline);

		return pipeline;
	}

	PipelineKey ResourceCache::BuildPipelineKey(const Material* material, const Mesh* mesh, bool wireframe)
	{
		PipelineKey key;

		if (mesh)
		{
			auto resource = mesh->GetResource();
			if (resource)
			{
				auto vertexData = resource->GetVertexData();
				if (vertexData)
					key.vertexLayoutHash = HashVertexLayout(vertexData->GetLayout());
			}
		}

		if (material)
		{
			key.materialType = material->GetType();
			key.cullMode = material->GetCullMode();
			key.blendMode = material->GetBlendMode();
			key.depthTest = material->GetDepthTest();
			key.depthWrite = (material->GetDepthTest() != RHI::DepthTestMode::None);
		}

		key.wireframe = m_Wireframe || wireframe;


		if (mesh)
		{
			// Detect skinned meshes by vertex layout
			auto resource = mesh->GetResource();
			if (resource)
			{
				auto vd = resource->GetVertexData();
				if (vd)
					key.skinned = vd->GetLayout().HasAttribute(RHI::VertexAttributeType::BlendIndices);
			}
		}

		return key;
	}

	void ResourceCache::InvalidatePipelines()
	{
		m_Pipelines.clear();
		OutputDebugStringA("ResourceCache: All pipelines invalidated\n");
	}

	void ResourceCache::SetWireframeMode(bool enable)
	{
		if (m_Wireframe != enable)
		{
			m_Wireframe = enable;
			InvalidatePipelines();  // Existing pipelines are now stale
		}
	}

	std::shared_ptr<RHI::IBuffer> ResourceCache::GetOrCreateConstantBuffer(const std::string& name, uint32_t sizeBytes)
	{
		//align to 16 bytes
		uint32_t alignedSize = (sizeBytes + 15u) & ~15u;



		auto it = m_ConstantBuffers.find(name);

		if (it != m_ConstantBuffers.end())
		{
			if (it->second.sizeBytes == alignedSize)
				return it->second.buffer;

			m_ConstantBuffers.erase(it);
		}
		RHI::BufferDesc desc;
		desc.type = RHI::BufferType::Uniform;
		desc.usage = RHI::BufferUsage::Dynamic;
		desc.size = alignedSize;
		desc.stride = 0;
		desc.debugName = name;

		auto buffer = m_Device->CreateBuffer(desc);
		if (!buffer || !buffer->IsValid())
		{
			OutputDebugStringA(("ERROR: ResourceCache failed to create CB: " + name + "\n").c_str());
			return nullptr;
		}

		m_ConstantBuffers[name] = { buffer, alignedSize };
		return buffer;
	}

	std::shared_ptr<RHI::IBuffer> ResourceCache::AcquireTransientBuffer(uint32_t sizeBytes, uint32_t stride, RHI::BufferType type)
	{
		//find a free compatible buffer
		for (auto& entry : m_TransientPool)
		{
			if (!entry.inUse
				&& entry.type == type
				&& entry.stride == stride
				&& entry.sizeBytes >= sizeBytes)
			{
				entry.inUse = true;
				return entry.buffer;
			}
		}


		RHI::BufferDesc desc;
		desc.type = type;
		desc.usage = RHI::BufferUsage::Dynamic;
		desc.size = sizeBytes;
		desc.stride = stride;
		desc.debugName = "TransientBuffer";

		auto buffer = m_Device->CreateBuffer(desc);
		if (!buffer || !buffer->IsValid())
		{
			OutputDebugStringA("ERROR: ResourceCache failed to create transient buffer\n");
			return nullptr;
		}

		m_TransientPool.push_back({ buffer, sizeBytes, stride, type, true });
		return buffer;
	}

	void ResourceCache::ReturnTransientBuffers()
	{
		for (auto& entry : m_TransientPool)
		{
			entry.inUse = false;
		}
	}

	size_t ResourceCache::HashVertexLayout(const VertexLayout& layout) const
	{
		size_t h = 0;
		auto combine = [&](size_t v) {h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };

		for (const auto& attr : layout.GetAttributes())
		{
			combine(std::hash<int>{}(static_cast<int>(attr.SemanticIndex)));
			combine(std::hash<int>{}(static_cast<int>(attr.Format)));
			combine(std::hash<uint32_t>{}(attr.Offset));
			combine(std::hash<uint32_t>{}(attr.Slot));
		}
		return h;
	}

	std::string ResourceCache::GetDebugInfo() const
	{
		std::ostringstream oss;
		oss << "ResourceCache:\n";
		oss << "  Pipelines:        " << m_Pipelines.size() << "\n";
		oss << "  Constant Buffers: " << m_ConstantBuffers.size() << "\n";
		oss << "  Transient Pool:   " << m_TransientPool.size() << "\n";
		oss << "  Wireframe:        " << (m_Wireframe ? "ON" : "OFF") << "\n";
		return oss.str();
	}



}