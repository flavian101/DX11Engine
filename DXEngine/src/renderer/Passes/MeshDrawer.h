#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderTypes.h"
#include <DirectXMath.h>
#include <memory>

namespace DXEngine
{
    class Mesh;
    class Material;
    namespace RHI { class IBuffer; class IPipeline; }

    // ==================================================================
    // MeshDrawer: "how ONE mesh gets drawn". Used by ForwardPass (x3) and UIPass.
    //
    //  Pass   = WHERE and WHEN to draw   (which targets, which list, which order)
    //  Drawer = HOW to draw one item     (pipeline, material, transform, geometry)
    //
    // Binding contract with the HLSL (common.hlsli):
    //   b0 TransformBuffer (WVP, Model, View, Projection, CameraPosition, Time)
    //   b1 BoneMatrices      b2 Material      b3 SceneLights      s0 sampler
    //   t0..t14 material textures (index == TextureSlot value)
    // ==================================================================
    class MeshDrawer
    {
    public:
        bool Initialize(FrameContext& ctx);          // create the shared constant buffers
        void BeginPass(FrameContext& ctx);           // per-pass state: camera matrices, sampler, lights

        void Draw(FrameContext& ctx, const DrawItem& item);                       // 3D item
        void DrawMesh(FrameContext& ctx, Mesh& mesh, Material& material,
                      uint32_t submeshIndex);                                     // pipeline+material+geometry only

        // UIPass changes a material's colour between draws, so the cached "last material" is stale.
        void InvalidateMaterialCache() { m_LastMaterial = nullptr; }

    private:
        void UploadTransform(const DirectX::XMFLOAT4X4& world);
        void UploadBones(const DrawItem& item);
        void BindMaterial(FrameContext& ctx, Material& material);
        void IssueDraw(FrameContext& ctx, Mesh& mesh, uint32_t submeshIndex);

        std::shared_ptr<RHI::IBuffer> m_TransformCB;
        std::shared_ptr<RHI::IBuffer> m_BoneCB;

        // cached "what is bound right now" -> skip redundant GPU calls. Reset in BeginPass().
        RHI::IPipeline* m_LastPipeline = nullptr;
        Material*       m_LastMaterial = nullptr;
        Mesh*           m_LastMesh     = nullptr;

        // per-pass copies of camera data (XMFLOAT4X4, not XMMATRIX: no alignment worries in members)
        DirectX::XMFLOAT4X4 m_View{}, m_Proj{};
        DirectX::XMFLOAT3   m_CameraPos{};
        float               m_Time = 0.0f;
        RHI::ICommandBuffer* m_Cmd = nullptr;
    };
}
