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

        // Depth-only pipelines (shadow maps). A normal pipeline leaves these at their defaults, so its key is unchanged.
        bool                   depthOnly  = false;   // no pixel shader, no colour writes
        int32_t                depthBias  = 0;       // rasterizer depth bias (in depth-buffer units)
        float                  slopeBias  = 0.0f;    // rasterizer slope-scaled depth bias
        bool                   scissor    = false;   // scissor test on (shadow tiles share one atlas)

        bool operator==(const PipelineKey& o) const
        {
            return program == o.program && vertexLayoutHash == o.vertexLayoutHash &&
                   cullMode == o.cullMode && blendMode == o.blendMode &&
                   depthTest == o.depthTest && topology == o.topology &&
                   depthWrite == o.depthWrite && wireframe == o.wireframe &&
                   depthOnly == o.depthOnly && depthBias == o.depthBias &&
                   slopeBias == o.slopeBias && scissor == o.scissor;
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
            mix(k.depthOnly ? 1u : 0u);
            mix(static_cast<size_t>(k.depthBias));
            mix(std::hash<float>{}(k.slopeBias));
            mix(k.scissor ? 1u : 0u);
            return h;
        }
    };

    // ==================================================================
    // ResourceCache: owns the long-lived GPU objects the RENDERER creates
    // (pipelines, shared constant / structured buffers, the default sampler).
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

        // A pipeline that only writes DEPTH: a vertex shader, no pixel shader, no colour target. Used by the shadow
        // pass. 'program' must be depth-only (ShaderProgram::CreateDepthOnly). The rasterizer bias is part of the
        // pipeline in D3D11, so it is part of the cache key; the scissor test is on because every shadow view is a
        // tile of ONE atlas and must not draw into its neighbour.
        std::shared_ptr<RHI::IPipeline> GetDepthOnlyPipeline(const Mesh& mesh,
                                                             const std::shared_ptr<ShaderProgram>& program,
                                                             RHI::CullMode cullMode,
                                                             int32_t depthBias, float slopeBias);

        // Named depth texture (a render target AND shader-readable: the shadow atlas). Same name + same shape => same
        // texture; a different shape REPLACES it. Returns nullptr when the device refuses.
        std::shared_ptr<RHI::ITexture> GetOrCreateDepthTexture(const std::string& name, uint32_t width, uint32_t height,
                                                               RHI::TextureFormat format = RHI::TextureFormat::D32_FLOAT);

        // Linear PCF + clamp, "reference <= stored depth" comparison. Bound to s1 for shadow lookups.
        RHI::ISampler* GetShadowSampler();

        // Named, dynamic constant buffer (size rounded up to 16 bytes). Same name => same buffer.
        std::shared_ptr<RHI::IBuffer> GetOrCreateConstantBuffer(const std::string& name, uint32_t sizeBytes);

        // Named, dynamic structured buffer: 'elementCount' elements of 'strideBytes' each, rewritten by the CPU and
        // read in shaders as StructuredBuffer<T> (bind it with ICommandBuffer::SetBuffer). Same name + same shape =>
        // same buffer; a different shape REPLACES it (users that still hold the old one keep it alive until they
        // let go). Returns nullptr when the device refuses (the reason goes to the debug output).
        std::shared_ptr<RHI::IBuffer> GetOrCreateStructuredBuffer(const std::string& name, uint32_t strideBytes, uint32_t elementCount);

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
        std::unordered_map<std::string, CBEntry> m_StructuredBuffers;   // same entry type: buffer + size in bytes
        std::unordered_map<std::string, std::shared_ptr<RHI::ITexture>> m_DepthTextures;
        std::shared_ptr<RHI::ISampler> m_DefaultSampler;
        std::shared_ptr<RHI::ISampler> m_ShadowSampler;
        bool m_Wireframe = false;   // was uninitialised before
    };
}
