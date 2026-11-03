#pragma once
#include "RHI/GraphicsDevice.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace DXEngine
{
    class Mesh;
    class Material;
    class ShaderProgram;

    // ==================================================================
    // PipelineKey: everything that makes two pipelines DIFFERENT.
    //
    // Fix vs. the old key: it had no shader identity, so two Lit materials that
    // compile to DIFFERENT shader variants (e.g. with / without a normal map)
    // collided and the second one drew with the first one's shader.
    // The ShaderProgram pointer is a perfect identity because ShaderCache returns
    // ONE shared instance per variant. (The cache entry below keeps the program
    // alive, so its address cannot be reused while the pipeline exists.)
    // ==================================================================
    struct PipelineKey
    {
        const ShaderProgram*   program = nullptr;
        size_t                 vertexLayoutHash = 0;
        RHI::CullMode          cullMode  = RHI::CullMode::Back;
        RHI::BlendMode         blendMode = RHI::BlendMode::Opaque;
        RHI::DepthTestMode     depthTest = RHI::DepthTestMode::Less;
        RHI::PrimitiveTopology topology  = RHI::PrimitiveTopology::TriangleList;
        bool                   depthWrite = true;
        bool                   wireframe  = false;

        bool operator==(const PipelineKey& o) const
        {
            return program == o.program && vertexLayoutHash == o.vertexLayoutHash &&
                   cullMode == o.cullMode && blendMode == o.blendMode &&
                   depthTest == o.depthTest && topology == o.topology &&
                   depthWrite == o.depthWrite && wireframe == o.wireframe;
        }
    };

    struct PipelineKeyHash
    {
        size_t operator()(const PipelineKey& k) const noexcept
        {
            size_t h = std::hash<const void*>{}(k.program);
            auto mix = [&](size_t v) { h ^= v + 0x9e3779b9 + (h << 6) + (h >> 2); };
            mix(k.vertexLayoutHash);
            mix(static_cast<size_t>(k.cullMode));
            mix(static_cast<size_t>(k.blendMode));
            mix(static_cast<size_t>(k.depthTest));
            mix(static_cast<size_t>(k.topology));
            mix(k.depthWrite ? 1u : 0u);
            mix(k.wireframe ? 1u : 0u);
            return h;
        }
    };

    // ==================================================================
    // ResourceCache: owns the long-lived GPU objects the RENDERER creates
    // (pipelines, shared constant buffers, the default sampler).
    // No pass creates raw RHI resources directly.
    //
    // Removed on purpose (nothing used them): transient buffer pool, BuildPipelineKey,
    // GetOrCreatePipeline(factory). Add back when instancing / a real consumer exists.
    // ==================================================================
    class ResourceCache
    {
    public:
        explicit ResourceCache(std::shared_ptr<RHI::IGraphicsDevice> device);

        bool Initialize();
        void Clear();

        // The ONE place a pipeline is described. Material = render state, Mesh = vertex
        // layout + topology, Program = shaders. Returns nullptr if the inputs are unusable.
        std::shared_ptr<RHI::IPipeline> GetPipeline(const Material& material,
                                                    const Mesh& mesh,
                                                    const std::shared_ptr<ShaderProgram>& program);

        // Named, dynamic constant buffer (size rounded up to 16 bytes). Same name => same buffer.
        std::shared_ptr<RHI::IBuffer> GetOrCreateConstantBuffer(const std::string& name, uint32_t sizeBytes);

        // Linear + wrap. Bound to s0 by the drawer.
        RHI::ISampler* GetDefaultSampler();

        void SetWireframeMode(bool enable);   // invalidates pipelines when the value changes
        bool GetWireframeMode() const { return m_Wireframe; }

        std::string GetDebugInfo() const;

    private:
        struct PipelineEntry
        {
            std::shared_ptr<RHI::IPipeline> pipeline;
            std::shared_ptr<ShaderProgram>  program;   // keeps the key's pointer valid
        };
        struct CBEntry
        {
            std::shared_ptr<RHI::IBuffer> buffer;
            uint32_t sizeBytes = 0;
        };

        std::shared_ptr<RHI::IGraphicsDevice> m_Device;
        std::unordered_map<PipelineKey, PipelineEntry, PipelineKeyHash> m_Pipelines;
        std::unordered_map<std::string, CBEntry> m_ConstantBuffers;
        std::shared_ptr<RHI::ISampler> m_DefaultSampler;
        bool m_Wireframe = false;   // was uninitialised before
    };
}
