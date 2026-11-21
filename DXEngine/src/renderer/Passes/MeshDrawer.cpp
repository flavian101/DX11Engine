#include "dxpch.h"
#include "MeshDrawer.h"
#include "../Cache/ResourceCache.h"
#include "../ShaderCache.h"
#include "camera/Camera.h"
#include "RHI/GraphicsDevice.h"
#include "shaders/ShaderProgram.h"
#include "utils/ConstantBufferTypes.h"     // TransfomBufferData, BoneMatrixBuffer, BindSlot
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"

namespace DXEngine
{
    using namespace DirectX;

    // Engine-wide buffers must not share a register with a material texture.
    static_assert(SRV_Lights >= kMaterialRegisterLimit,
                  "SRV_Lights collides with the material texture slots (t0..t31)");
    static_assert(SRV_Lights < SRV_ClusterRanges && SRV_ClusterRanges < SRV_LightIndices, "engine-wide SRV slots are in order");
    static_assert(SRV_LightIndices < SRV_ShadowAtlas && SRV_ShadowAtlas < SRV_ShadowViews, "engine-wide SRV slots are in order");

    bool MeshDrawer::Initialize(FrameContext& ctx)
    {
        auto& cache = *ctx.services->resources;
        m_TransformCB = cache.GetOrCreateConstantBuffer("Forward.Transform", sizeof(TransfomBufferData));
        m_BoneCB      = cache.GetOrCreateConstantBuffer("Forward.Bones",     sizeof(BoneMatrixBuffer));
        return m_TransformCB && m_BoneCB;
    }

    void MeshDrawer::BeginPass(FrameContext& ctx)
    {
        m_Cmd          = ctx.cmd;
        m_LastPipeline = nullptr;
        m_LastMaterial = nullptr;
        m_LastMesh     = nullptr;
        m_BonesAreIdentity = false;                    // another pass may have written b1
        m_Time         = ctx.totalTime;
        m_Kind         = DrawKind::Scene;
        m_ReceiveShadows = 1.0f;

        if (ctx.camera)
        {
            XMStoreFloat4x4(&m_View, ctx.camera->GetViewMatrix());
            XMStoreFloat4x4(&m_Proj, ctx.camera->GetProjectionMatrix());
            m_CameraPos = ctx.camera->GetPositionFloat3();
        }

        // Bindings that stay the same for the whole pass are set ONCE here.
        m_Cmd->SetConstantBuffer(m_TransformCB.get(), CB_Transform, RHI::ShaderStage::Vertex);
        m_Cmd->SetConstantBuffer(m_TransformCB.get(), CB_Transform, RHI::ShaderStage::Pixel);   // PS reads CameraPosition
        m_Cmd->SetConstantBuffer(m_BoneCB.get(),      CB_Bones,     RHI::ShaderStage::Vertex);

        if (ctx.lightBuffer)
            m_Cmd->SetConstantBuffer(ctx.lightBuffer, CB_Scene_Lights, RHI::ShaderStage::Pixel);

        // The point + spot lights. Bound unconditionally: a null buffer UNBINDS the slot, so the pixel shader can
        // never read a buffer left over from an earlier frame.
        m_Cmd->SetBuffer(ctx.localLightBuffer, SRV_Lights, RHI::ShaderStage::Pixel);

        // Which of those lights reach which part of the screen (the cluster grid).
        if (ctx.clusterBuffer)
            m_Cmd->SetConstantBuffer(ctx.clusterBuffer, CB_Cluster_Data, RHI::ShaderStage::Pixel);
        m_Cmd->SetBuffer(ctx.clusterRangeBuffer, SRV_ClusterRanges, RHI::ShaderStage::Pixel);
        m_Cmd->SetBuffer(ctx.lightIndexBuffer,   SRV_LightIndices,  RHI::ShaderStage::Pixel);

        if (RHI::ISampler* sampler = ctx.services->resources->GetDefaultSampler())
            m_Cmd->SetSampler(sampler, 0, RHI::ShaderStage::Pixel);

        // Shadows. The atlas and the views are bound unconditionally for the same reason as the lights: null UNBINDS, so
        // a frame without shadows can never read last frame's atlas (and the shadow pass can always bind it as a target).
        m_Cmd->SetTexture(ctx.shadows.atlas,       SRV_ShadowAtlas, RHI::ShaderStage::Pixel);
        m_Cmd->SetBuffer(ctx.shadows.viewBuffer,   SRV_ShadowViews, RHI::ShaderStage::Pixel);
        if (ctx.shadows.constantBuffer)
            m_Cmd->SetConstantBuffer(ctx.shadows.constantBuffer, CB_Shadow_Data, RHI::ShaderStage::Pixel);
        if (RHI::ISampler* shadowSampler = ctx.services->resources->GetShadowSampler())
            m_Cmd->SetSampler(shadowSampler, 1, RHI::ShaderStage::Pixel);
    }

    // ------------------------------------------------------------------
    // Shadow casters
    // ------------------------------------------------------------------
    void MeshDrawer::BeginDepthPass(FrameContext& ctx, int32_t depthBias, float slopeBias)
    {
        m_Cmd          = ctx.cmd;
        m_LastPipeline = nullptr;
        m_LastMaterial = nullptr;
        m_LastMesh     = nullptr;
        m_BonesAreIdentity = false;                    // another pass may have written b1
        m_Kind         = DrawKind::ShadowCaster;
        m_DepthBias    = depthBias;
        m_SlopeBias    = slopeBias;
        m_DepthMesh    = nullptr;
        m_DepthPipeline = nullptr;
        m_Time         = ctx.totalTime;
        m_ReceiveShadows = 0.0f;
        m_CameraPos    = XMFLOAT3{ 0, 0, 0 };

        // the caster shader reads b0 (WVP, or View * Projection for skinned meshes) and b1 (bones): nothing else
        m_Cmd->SetConstantBuffer(m_TransformCB.get(), CB_Transform, RHI::ShaderStage::Vertex);
        m_Cmd->SetConstantBuffer(m_BoneCB.get(),      CB_Bones,     RHI::ShaderStage::Vertex);
    }

    void MeshDrawer::SetDepthViewProj(const XMFLOAT4X4& lightViewProj)
    {
        // The vertex shaders compute WVP = World * View * Projection (skinned meshes too: World is their Animator's).
        // Putting the light's matrix in View and the identity in Projection gives all of them the light's clip space.
        m_View = lightViewProj;
        XMStoreFloat4x4(&m_Proj, XMMatrixIdentity());
    }

    RHI::IPipeline* MeshDrawer::DepthPipelineFor(FrameContext& ctx, Mesh& mesh, RHI::CullMode cull)
    {
        if (&mesh == m_DepthMesh && cull == m_DepthCull)
            return m_DepthPipeline;                                   // casters are sorted by mesh: this is the usual case

        m_DepthMesh = &mesh;
        m_DepthCull = cull;
        m_DepthPipeline = nullptr;

        const auto& resource = mesh.GetResource();
        if (!resource || !resource->GetVertexData()) return nullptr;

        auto& svc = *ctx.services;
        const VertexLayout& layout = resource->GetVertexData()->GetLayout();
        std::shared_ptr<ShaderProgram> program = svc.shaders->GetShadowCasterShader(layout);
        std::shared_ptr<RHI::IPipeline> pipeline = svc.resources->GetDepthOnlyPipeline(mesh, program, cull, m_DepthBias, m_SlopeBias);
        m_DepthPipeline = pipeline.get();                            // the ResourceCache keeps it alive
        return m_DepthPipeline;
    }

    bool MeshDrawer::DrawDepth(FrameContext& ctx, const DrawItem& item)
    {
        if (!item.mesh || !m_Cmd) return false;

        // double-sided materials (cull none) cast from both faces; everything else is drawn the way the camera sees it
        const RHI::CullMode cull = item.material ? item.material->GetCullMode() : RHI::CullMode::Back;
        RHI::IPipeline* pipeline = DepthPipelineFor(ctx, *item.mesh, cull);
        if (!pipeline) return false;

        if (pipeline != m_LastPipeline)
        {
            m_Cmd->SetPipeline(pipeline);
            m_LastPipeline = pipeline;
        }

        SubmitGeometry(ctx, item);
        return true;
    }

    void MeshDrawer::SubmitGeometry(FrameContext& ctx, const DrawItem& item)
    {
        UploadBonesFor(item);

        UploadTransform(item.world);
        IssueDraw(ctx, *item.mesh, item.submeshIndex);
    }

    // ------------------------------------------------------------------
    // Depth pre-pass
    // ------------------------------------------------------------------
    void MeshDrawer::BeginPrepass(FrameContext& ctx)
    {
        m_Cmd          = ctx.cmd;
        m_LastPipeline = nullptr;
        m_LastMaterial = nullptr;
        m_LastMesh     = nullptr;
        m_BonesAreIdentity = false;                    // another pass may have written b1
        m_Kind         = DrawKind::Prepass;
        m_Time         = ctx.totalTime;
        m_ReceiveShadows = 0.0f;
        m_PrepassMaterial = nullptr;
        m_PrepassMesh     = nullptr;
        m_PrepassPipeline = nullptr;

        // the colour pass uploads exactly these matrices: the same inputs to the same vertex shader give the same depth
        if (ctx.camera)
        {
            XMStoreFloat4x4(&m_View, ctx.camera->GetViewMatrix());
            XMStoreFloat4x4(&m_Proj, ctx.camera->GetProjectionMatrix());
            m_CameraPos = ctx.camera->GetPositionFloat3();
        }

        m_Cmd->SetConstantBuffer(m_TransformCB.get(), CB_Transform, RHI::ShaderStage::Vertex);
        m_Cmd->SetConstantBuffer(m_BoneCB.get(),      CB_Bones,     RHI::ShaderStage::Vertex);
    }

    RHI::IPipeline* MeshDrawer::PrepassPipelineFor(FrameContext& ctx, Mesh& mesh, Material& material)
    {
        if (&material == m_PrepassMaterial && &mesh == m_PrepassMesh)
            return m_PrepassPipeline;

        m_PrepassMaterial = &material;
        m_PrepassMesh     = &mesh;
        m_PrepassPipeline = nullptr;

        std::shared_ptr<ShaderProgram> program = ResolveProgram(ctx, mesh, material);
        if (!program) return nullptr;
        std::shared_ptr<RHI::IPipeline> pipeline = ctx.services->resources->GetPrepassPipeline(material, mesh, program);
        m_PrepassPipeline = pipeline.get();                           // the ResourceCache keeps it alive
        return m_PrepassPipeline;
    }

    bool MeshDrawer::DrawPrepass(FrameContext& ctx, const DrawItem& item)
    {
        if (!item.mesh || !item.material || !m_Cmd) return false;

        RHI::IPipeline* pipeline = PrepassPipelineFor(ctx, *item.mesh, *item.material);
        if (!pipeline) return false;

        if (pipeline != m_LastPipeline)
        {
            m_Cmd->SetPipeline(pipeline);
            m_LastPipeline = pipeline;
        }

        SubmitGeometry(ctx, item);
        return true;
    }

    // ------------------------------------------------------------------
    void MeshDrawer::Draw(FrameContext& ctx, const DrawItem& item)
    {
        if (!item.mesh || !item.material || !m_Cmd) return;

        m_ReceiveShadows = item.receivesShadows ? 1.0f : 0.0f;

        UploadBonesFor(item);

        UploadTransform(item.world);
        DrawMesh(ctx, *item.mesh, *item.material, item.submeshIndex, item.depthPrepassed);
    }

    // The shader variant of a material on a mesh: the colour pass and the depth pre-pass must pick the SAME program, so
    // they both come through here.
    //    PERF NOTE: GetShaderForMaterial builds a string key per call. Fine for hundreds of draws; the pre-pass calls it
    //    once per run of equal (material, mesh) items. If the profiler ever points here, cache the program on the Material.
    std::shared_ptr<ShaderProgram> MeshDrawer::ResolveProgram(FrameContext& ctx, Mesh& mesh, Material& material)
    {
        const auto& resource = mesh.GetResource();
        if (!resource || !resource->GetVertexData()) return nullptr;

        auto& svc = *ctx.services;
        const VertexLayout& layout = resource->GetVertexData()->GetLayout();
        std::shared_ptr<ShaderProgram> program = svc.shaders->GetShaderForMaterial(layout, &material);
        if (!program || !program->IsValid())
            program = svc.shaders->GetFallbackShader(material.GetShadingModel());
        return program;
    }

    void MeshDrawer::DrawMesh(FrameContext& ctx, Mesh& mesh, Material& material, uint32_t submeshIndex, bool afterPrepass)
    {
        auto& svc = *ctx.services;

        // 1) shader variant -> pipeline (both cached)
        std::shared_ptr<ShaderProgram> program = ResolveProgram(ctx, mesh, material);
        if (!program) return;

        std::shared_ptr<RHI::IPipeline> pipeline = svc.resources->GetPipeline(material, mesh, program, afterPrepass);
        if (!pipeline) return;

        if (pipeline.get() != m_LastPipeline)
        {
            m_Cmd->SetPipeline(pipeline.get());
            m_LastPipeline = pipeline.get();
            ++ctx.stats.pipelineChanges;
        }

        // 2) material constants + textures
        if (&material != m_LastMaterial)
        {
            BindMaterial(ctx, material);
            m_LastMaterial = &material;
            ++ctx.stats.materialChanges;
        }

        // 3) geometry
        IssueDraw(ctx, mesh, submeshIndex);
    }

    // ------------------------------------------------------------------
    void MeshDrawer::UploadTransform(const XMFLOAT4X4& worldF)
    {
        const XMMATRIX world = XMLoadFloat4x4(&worldF);
        const XMMATRIX view  = XMLoadFloat4x4(&m_View);
        const XMMATRIX proj  = XMLoadFloat4x4(&m_Proj);

        // C++ (DirectXMath) is row-vector; HLSL cbuffers default to column-major -> transpose on upload.
        TransfomBufferData cb;
        cb.WVP        = XMMatrixTranspose(world * view * proj);
        cb.Model      = XMMatrixTranspose(world);
        cb.View       = XMMatrixTranspose(view);
        cb.Projection = XMMatrixTranspose(proj);
        cb.cameraPosition = m_CameraPos;
        cb.time       = m_Time;
        cb.receiveShadows = m_ReceiveShadows;
        cb.padding[0] = cb.padding[1] = cb.padding[2] = 0.0f;
        m_TransformCB->Update(&cb, sizeof(cb), 0);
    }

    void MeshDrawer::UploadBonesFor(const DrawItem& item)
    {
        if (item.IsSkinned())
        {
            UploadBones(item);
            return;
        }

        const auto& resource = item.mesh ? item.mesh->GetResource() : nullptr;
        if (!resource || !resource->GetVertexData() ||
            !resource->GetVertexData()->GetLayout().HasAttribute(RHI::VertexAttributeType::BlendWeights))
            return;                                              // a static mesh: its shader never reads b1
        if (m_BonesAreIdentity)
            return;

        BoneMatrixBuffer data;
        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixIdentity());
        for (size_t i = 0; i < 128; ++i) data.boneMatrices[i] = identity;
        m_BoneCB->Update(&data, sizeof(data), 0);
        m_BonesAreIdentity = true;
    }

    void MeshDrawer::UploadBones(const DrawItem& item)
    {
        m_BonesAreIdentity = false;
        BoneMatrixBuffer data;
        XMFLOAT4X4 identity;
        XMStoreFloat4x4(&identity, XMMatrixIdentity());

        const size_t n = std::min<size_t>(item.boneMatrices->size(), 128);
        for (size_t i = 0; i < n; ++i)   data.boneMatrices[i] = (*item.boneMatrices)[i];   // already GPU-ready (AnimationEvaluator)
        for (size_t i = n; i < 128; ++i) data.boneMatrices[i] = identity;

        m_BoneCB->Update(&data, sizeof(data), 0);
    }

    void MeshDrawer::BindMaterial(FrameContext& ctx, Material& material)
    {
        material.UpdateConstantBuffer(ctx.services->device.get());   // no-op unless the material is dirty

        if (RHI::IBuffer* cb = material.GetConstantBuffer())
        {
            m_Cmd->SetConstantBuffer(cb, CB_Material, RHI::ShaderStage::Vertex);   // VS uses texture scale/offset
            m_Cmd->SetConstantBuffer(cb, CB_Material, RHI::ShaderStage::Pixel);
        }

        // textures vector is indexed by TextureSlot, which equals the HLSL register number (the slot table guarantees it:
        // the Texture2D declarations the shaders get are generated from the same table)
        const auto& textures = material.GetTextures();
        for (uint32_t slot = 0; slot < textures.size(); ++slot)
            if (textures[slot])
                m_Cmd->SetTexture(textures[slot].get(), slot, RHI::ShaderStage::Pixel);
    }

    void MeshDrawer::IssueDraw(FrameContext& ctx, Mesh& mesh, uint32_t submeshIndex)
    {
        if (&mesh != m_LastMesh)
        {
            mesh.Bind(m_Cmd);                       // uploads to GPU on first use, binds VB + IB
            m_LastMesh = &mesh;
        }
        if (!mesh.GetBuffers().IsValid()) return;   // upload failed: skip instead of drawing garbage

        const MeshResource& res = *mesh.GetResource();

        if (res.HasIndices())
        {
            uint32_t start = 0;
            uint32_t count = static_cast<uint32_t>(res.GetIndexData()->GetIndexCount());

            if (res.HasSubmeshes() && submeshIndex < res.GetSubMeshCount())
            {
                const SubMesh& sm = res.GetSubMesh(submeshIndex);
                start = sm.indexStart;
                count = sm.indexCount;              // submesh indices are absolute -> baseVertex 0
            }
            m_Cmd->DrawIndexed(count, start, 0);
            if (m_Kind == DrawKind::Scene) ctx.stats.trianglesRendered += count / 3;
        }
        else
        {
            const uint32_t vertices = static_cast<uint32_t>(res.GetVertexData()->GetVertexCount());
            m_Cmd->Draw(vertices, 0);
            if (m_Kind == DrawKind::Scene) ctx.stats.trianglesRendered += vertices / 3;
        }
        // shadow caster and pre-pass draws are their own numbers: they are not scene draw calls, and adding them would
        // hide what the scene itself costs
        switch (m_Kind)
        {
        case DrawKind::ShadowCaster: ++ctx.stats.shadowCasterDraws; break;
        case DrawKind::Prepass:      ++ctx.stats.depthPrepassDraws; break;
        default:                     ++ctx.stats.drawCalls;         break;
        }
    }
}
