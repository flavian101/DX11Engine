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
    static_assert(SRV_Lights >= static_cast<uint32_t>(TextureSlot::MaxTextureSlots),
                  "SRV_Lights collides with the material texture slots (t0..t31)");

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
        m_Time         = ctx.totalTime;

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

        if (RHI::ISampler* sampler = ctx.services->resources->GetDefaultSampler())
            m_Cmd->SetSampler(sampler, 0, RHI::ShaderStage::Pixel);
    }

    // ------------------------------------------------------------------
    void MeshDrawer::Draw(FrameContext& ctx, const DrawItem& item)
    {
        if (!item.mesh || !item.material || !m_Cmd) return;

        if (item.IsSkinned())
            UploadBones(item);

        if (item.IsInstanced())
        {
            // The current HLSL has no per-instance vertex input, so true GPU instancing is not
            // possible yet. Correct-but-slower fallback: one draw per instance.
            // ASSUMPTION: instance transforms are placed inside the model's space
            //             (final = instance * model). Verify against how you author instances.
            const XMMATRIX model = XMLoadFloat4x4(&item.world);
            for (const XMFLOAT4X4& inst : *item.instanceTransforms)
            {
                XMFLOAT4X4 world;
                XMStoreFloat4x4(&world, XMLoadFloat4x4(&inst) * model);
                UploadTransform(world);
                DrawMesh(ctx, *item.mesh, *item.material, item.submeshIndex);
            }
            return;
        }

        UploadTransform(item.world);
        DrawMesh(ctx, *item.mesh, *item.material, item.submeshIndex);
    }

    void MeshDrawer::DrawMesh(FrameContext& ctx, Mesh& mesh, Material& material, uint32_t submeshIndex)
    {
        auto& svc = *ctx.services;
        const auto& resource = mesh.GetResource();
        if (!resource || !resource->GetVertexData()) return;

        // 1) shader variant -> pipeline (both cached)
        //    PERF NOTE: GetShaderForMaterial builds a string key per call. Fine for hundreds of
        //    draws; if the profiler ever points here, cache the program on the Material.
        const VertexLayout& layout = resource->GetVertexData()->GetLayout();
        std::shared_ptr<ShaderProgram> program = svc.shaders->GetShaderForMaterial(layout, &material);
        if (!program || !program->IsValid())
            program = svc.shaders->GetFallbackShader(material.GetType());

        std::shared_ptr<RHI::IPipeline> pipeline = svc.resources->GetPipeline(material, mesh, program);
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
        m_TransformCB->Update(&cb, sizeof(cb), 0);
    }

    void MeshDrawer::UploadBones(const DrawItem& item)
    {
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

        // textures vector is indexed by TextureSlot, which equals the HLSL register number
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
            ctx.stats.trianglesRendered += count / 3;
        }
        else
        {
            const uint32_t vertices = static_cast<uint32_t>(res.GetVertexData()->GetVertexCount());
            m_Cmd->Draw(vertices, 0);
            ctx.stats.trianglesRendered += vertices / 3;
        }
        ++ctx.stats.drawCalls;
    }
}
