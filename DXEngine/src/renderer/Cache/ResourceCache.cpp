#include "dxpch.h"
#include "ResourceCache.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "shaders/ShaderProgram.h"
#include <sstream>

namespace DXEngine
{
    static size_t HashLayout(const VertexLayout& layout)
    {
        size_t h = 0;
        auto mix = [&](size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
        for (const auto& a : layout.GetAttributes())
        {
            mix(static_cast<size_t>(a.Type));
            mix(static_cast<size_t>(a.SemanticIndex));
            mix(static_cast<size_t>(a.Format));
            mix(a.Offset);
            mix(a.Slot);
            mix(a.PerInstance ? 1u : 0u);
        }
        return h;
    }

    ResourceCache::ResourceCache(std::shared_ptr<RHI::IGraphicsDevice> device) : m_Device(std::move(device)) {}

    bool ResourceCache::Initialize()
    {
        if (!m_Device) { OutputDebugStringA("ERROR: ResourceCache::Initialize - null device\n"); return false; }
        m_Pipelines.reserve(64);
        m_ConstantBuffers.reserve(16);
        return true;
    }

    void ResourceCache::Clear()
    {
        m_Pipelines.clear();
        m_ConstantBuffers.clear();
        m_DefaultSampler.reset();
    }

    std::shared_ptr<RHI::IPipeline> ResourceCache::GetPipeline(const Material& material,
                                                               const Mesh& mesh,
                                                               const std::shared_ptr<ShaderProgram>& program)
    {
        const auto& resource = mesh.GetResource();
        if (!program || !program->IsValid() || !resource || !resource->GetVertexData())
            return nullptr;

        const VertexLayout& layout = resource->GetVertexData()->GetLayout();

        PipelineKey key;
        key.program          = program.get();
        key.vertexLayoutHash = HashLayout(layout);
        key.cullMode         = material.GetCullMode();
        key.blendMode        = material.GetBlendMode();
        key.depthTest        = material.GetDepthTest();
        key.topology         = resource->GetTopology();
        key.wireframe        = m_Wireframe;
        // Blended geometry must not write depth, otherwise whatever is drawn behind it
        // (back-to-front order) gets rejected by the depth test.
        key.depthWrite       = key.blendMode == RHI::BlendMode::Opaque &&
                               key.depthTest != RHI::DepthTestMode::None;

        if (auto it = m_Pipelines.find(key); it != m_Pipelines.end())
            return it->second.pipeline;                       // cache hit

        // ---- cache miss: describe and create ----
        RHI::PipelineDesc desc;
        desc.vertexShader        = program->GetVertexShader();
        desc.pixelShader         = program->GetPixelShader();
        desc.inputLayout         = layout.GetAttributes();
        desc.rasterizer.cullMode = key.cullMode;
        desc.rasterizer.wireframe = key.wireframe;
        desc.depthTest           = key.depthTest;
        desc.depthWrite          = key.depthWrite;
        desc.blendMode           = key.blendMode;
        desc.topology            = key.topology;
        desc.debugName           = "Pipeline_" + material.GetName();

        auto pipeline = m_Device->CreatePipeline(desc);
        if (!pipeline || !pipeline->IsValid())
        {
            OutputDebugStringA(("ERROR: ResourceCache: pipeline creation failed for " + material.GetName() + "\n").c_str());
            return nullptr;
        }

        m_Pipelines.emplace(key, PipelineEntry{ pipeline, program });
        return pipeline;
    }

    std::shared_ptr<RHI::IBuffer> ResourceCache::GetOrCreateConstantBuffer(const std::string& name, uint32_t sizeBytes)
    {
        const uint32_t aligned = (sizeBytes + 15u) & ~15u;

        auto it = m_ConstantBuffers.find(name);
        if (it != m_ConstantBuffers.end() && it->second.sizeBytes == aligned)
            return it->second.buffer;

        RHI::BufferDesc desc;
        desc.type      = RHI::BufferType::Uniform;
        desc.usage     = RHI::BufferUsage::Dynamic;
        desc.size      = aligned;
        desc.stride    = 0;
        desc.debugName = name;

        auto buffer = m_Device->CreateBuffer(desc);
        if (!buffer || !buffer->IsValid())
        {
            OutputDebugStringA(("ERROR: ResourceCache: failed to create constant buffer " + name + "\n").c_str());
            return nullptr;
        }
        m_ConstantBuffers[name] = { buffer, aligned };
        return buffer;
    }

    RHI::ISampler* ResourceCache::GetDefaultSampler()
    {
        if (!m_DefaultSampler)
        {
            RHI::SamplerDesc desc;                 // defaults: linear filtering, wrap addressing
            desc.debugName = "DefaultLinearWrap";
            m_DefaultSampler = m_Device->CreateSampler(desc);
        }
        return m_DefaultSampler.get();
    }

    void ResourceCache::SetWireframeMode(bool enable)
    {
        if (m_Wireframe == enable) return;
        m_Wireframe = enable;
        m_Pipelines.clear();   // wireframe is part of the key; old entries would just waste memory
    }

    std::string ResourceCache::GetDebugInfo() const
    {
        std::ostringstream oss;
        oss << "ResourceCache: " << m_Pipelines.size() << " pipelines, "
            << m_ConstantBuffers.size() << " constant buffers, wireframe "
            << (m_Wireframe ? "ON" : "OFF") << "\n";
        return oss.str();
    }
}
