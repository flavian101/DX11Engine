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
        m_StructuredBuffers.clear();
        m_DepthTextures.clear();
        m_DefaultSampler.reset();
        m_ShadowSampler.reset();
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
        const RenderState& state = material.GetRenderState();      // HOW this material is drawn
        key.cullMode         = state.cull;
        key.blendMode        = state.blend;
        key.depthTest        = state.depthTest;
        key.topology         = resource->GetTopology();
        key.wireframe        = m_Wireframe;
        key.depthWrite       = ResolveDepthWrite(state);                 // (blended geometry does not write depth: see RenderState)

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

    std::shared_ptr<RHI::IPipeline> ResourceCache::GetDepthOnlyPipeline(const Mesh& mesh,
                                                                        const std::shared_ptr<ShaderProgram>& program,
                                                                        RHI::CullMode cullMode,
                                                                        int32_t depthBias, float slopeBias)
    {
        const auto& resource = mesh.GetResource();
        if (!program || !program->IsValid() || !program->IsDepthOnly() || !resource || !resource->GetVertexData())
            return nullptr;

        const VertexLayout& layout = resource->GetVertexData()->GetLayout();

        PipelineKey key;
        key.program          = program.get();
        key.vertexLayoutHash = HashLayout(layout);
        key.cullMode         = cullMode;
        key.blendMode        = RHI::BlendMode::Opaque;
        key.depthTest        = RHI::DepthTestMode::Less;
        key.topology         = resource->GetTopology();
        key.depthWrite       = true;
        key.wireframe        = false;          // a wireframe SHADOW map would only show the edges of the casters
        key.depthOnly        = true;
        key.depthBias        = depthBias;
        key.slopeBias        = slopeBias;
        key.scissor          = true;

        if (auto it = m_Pipelines.find(key); it != m_Pipelines.end())
            return it->second.pipeline;

        RHI::PipelineDesc desc;
        desc.vertexShader         = program->GetVertexShader();
        desc.pixelShader          = nullptr;     // depth only
        desc.inputLayout          = layout.GetAttributes();
        desc.rasterizer.cullMode  = key.cullMode;
        desc.rasterizer.depthBias = depthBias;
        desc.rasterizer.slopeScaledDepthBias = slopeBias;
        desc.rasterizer.scissorEnable = true;
        desc.depthTest            = key.depthTest;
        desc.depthWrite           = true;
        desc.blendMode            = RHI::BlendMode::Opaque;
        desc.topology             = key.topology;
        desc.debugName            = "Pipeline_ShadowDepth";

        std::shared_ptr<RHI::IPipeline> pipeline;
        try
        {
            pipeline = m_Device->CreatePipeline(desc);
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA((std::string("ERROR: ResourceCache: depth-only pipeline creation failed: ") + e.what() + "\n").c_str());
            return nullptr;
        }
        if (!pipeline || !pipeline->IsValid())
        {
            OutputDebugStringA("ERROR: ResourceCache: depth-only pipeline creation failed\n");
            return nullptr;
        }

        m_Pipelines.emplace(key, PipelineEntry{ pipeline, program });
        return pipeline;
    }

    std::shared_ptr<RHI::ITexture> ResourceCache::GetOrCreateDepthTexture(const std::string& name, uint32_t width, uint32_t height,
                                                                         RHI::TextureFormat format)
    {
        if (width == 0 || height == 0) return nullptr;

        auto it = m_DepthTextures.find(name);
        if (it != m_DepthTextures.end() && it->second && it->second->GetWidth() == width &&
            it->second->GetHeight() == height && it->second->GetFormat() == format)
            return it->second;

        RHI::TextureDesc desc;
        desc.type            = RHI::TextureType::Texture2D;
        desc.format          = format;
        desc.width           = width;
        desc.height          = height;
        desc.isDepthStencil  = true;      // can be rendered to ...
        desc.isRenderTarget  = false;
        desc.debugName       = name;      // ... and (always) read by shaders: the D3D11 backend adds the SRV

        // The D3D11 backend throws when the device refuses (out of memory, unsupported size).
        std::shared_ptr<RHI::ITexture> texture;
        try
        {
            texture = m_Device->CreateTexture(desc);
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("ERROR: ResourceCache: failed to create depth texture " + name + ": " + e.what() + "\n").c_str());
            return nullptr;
        }
        if (!texture || !texture->IsValid())
        {
            OutputDebugStringA(("ERROR: ResourceCache: failed to create depth texture " + name + "\n").c_str());
            return nullptr;
        }
        m_DepthTextures[name] = texture;
        return texture;
    }

    RHI::ISampler* ResourceCache::GetShadowSampler()
    {
        if (!m_ShadowSampler)
        {
            RHI::SamplerDesc desc;
            desc.minFilter  = RHI::FilterMode::Linear;       // linear + comparison = the hardware does a 2x2 PCF per tap
            desc.magFilter  = RHI::FilterMode::Linear;
            desc.mipFilter  = RHI::FilterMode::Point;
            desc.addressU   = RHI::AddressMode::Clamp;
            desc.addressV   = RHI::AddressMode::Clamp;
            desc.addressW   = RHI::AddressMode::Clamp;
            desc.enableComparison = true;
            desc.comparisonFunc   = RHI::ComparisonFunc::LessEqual;   // lit when  reference depth <= stored depth
            desc.debugName  = "ShadowComparison";
            m_ShadowSampler = m_Device->CreateSampler(desc);
        }
        return m_ShadowSampler.get();
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

    std::shared_ptr<RHI::IBuffer> ResourceCache::GetOrCreateStructuredBuffer(const std::string& name, uint32_t strideBytes, uint32_t elementCount)
    {
        const uint32_t sizeBytes = strideBytes * elementCount;

        auto it = m_StructuredBuffers.find(name);
        if (it != m_StructuredBuffers.end() && it->second.sizeBytes == sizeBytes && it->second.buffer->GetStride() == strideBytes)
            return it->second.buffer;

        RHI::BufferDesc desc;
        desc.type      = RHI::BufferType::Storage;
        desc.usage     = RHI::BufferUsage::Dynamic;
        desc.size      = sizeBytes;
        desc.stride    = strideBytes;
        desc.debugName = name;

        // The RHI refuses a bad description (stride rules, size 0) by throwing, with the reason in the message.
        std::shared_ptr<RHI::IBuffer> buffer;
        try
        {
            buffer = m_Device->CreateBuffer(desc);
        }
        catch (const std::exception& e)
        {
            OutputDebugStringA(("ERROR: ResourceCache: failed to create structured buffer " + name + ": " + e.what() + "\n").c_str());
            return nullptr;
        }

        if (!buffer || !buffer->IsValid())
        {
            OutputDebugStringA(("ERROR: ResourceCache: failed to create structured buffer " + name + "\n").c_str());
            return nullptr;
        }
        m_StructuredBuffers[name] = { buffer, sizeBytes };
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
            << m_ConstantBuffers.size() << " constant buffers, "
            << m_StructuredBuffers.size() << " structured buffers, "
            << m_DepthTextures.size() << " depth textures, wireframe "
            << (m_Wireframe ? "ON" : "OFF") << "\n";
        return oss.str();
    }
}
