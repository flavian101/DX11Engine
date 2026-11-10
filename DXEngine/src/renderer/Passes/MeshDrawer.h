#pragma once
#include "../Core/FrameContext.h"
#include "../Core/RenderTypes.h"
#include "RHI/GraphicsTypes.h"   // RHI::CullMode
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
    //   b0 TransformBuffer (WVP, Model, View, Projection, CameraPosition, Time, ReceiveShadows)
    //   b1 BoneMatrices      b2 Material      b3 SceneLights      b5 ShadowData      s0 sampler   s1 shadow sampler
    //   t0..t31 material textures (index == TextureSlot value)
    //   t32.. engine-wide: lights, clusters, shadow atlas (t35) + shadow views (t36)
    //
    // The same drawer also draws shadow CASTERS (BeginDepthPass / DrawDepth): depth only, through the shadow caster
    // vertex shader, with the light's view-projection standing in for the camera.
    // ==================================================================
    class MeshDrawer
    {
    public:
        bool Initialize(FrameContext& ctx);          // create the shared constant buffers
        void BeginPass(FrameContext& ctx);           // per-pass state: camera matrices, sampler, lights

        void Draw(FrameContext& ctx, const DrawItem& item);                       // 3D item
        void DrawMesh(FrameContext& ctx, Mesh& mesh, Material& material,
                      uint32_t submeshIndex);                                     // pipeline+material+geometry only

        // ---- shadow pass: depth-only drawing ----
        // BeginDepthPass binds only what the caster shader reads (b0, b1). depthBias / slopeBias are the rasterizer's.
        // SetDepthViewProj changes the "camera" between views: it is the light's world -> clip matrix.
        void BeginDepthPass(FrameContext& ctx, int32_t depthBias, float slopeBias);
        void SetDepthViewProj(const DirectX::XMFLOAT4X4& lightViewProj);
        bool DrawDepth(FrameContext& ctx, const DrawItem& item);                  // true when something was drawn

        // UIPass changes a material's colour between draws, so the cached "last material" is stale.
        void InvalidateMaterialCache() { m_LastMaterial = nullptr; }

    private:
        void UploadTransform(const DirectX::XMFLOAT4X4& world);
        void UploadBones(const DrawItem& item);
        void BindMaterial(FrameContext& ctx, Material& material);
        void IssueDraw(FrameContext& ctx, Mesh& mesh, uint32_t submeshIndex);
        RHI::IPipeline* DepthPipelineFor(FrameContext& ctx, Mesh& mesh, RHI::CullMode cull);

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
        float               m_ReceiveShadows = 1.0f;     // per draw: DrawItem::receivesShadows
        RHI::ICommandBuffer* m_Cmd = nullptr;

        // depth-only drawing (shadow pass)
        bool                m_DepthPass = false;         // draws are counted as shadow caster draws, not as scene draws
        int32_t             m_DepthBias = 0;
        float               m_SlopeBias = 0.0f;
        const Mesh*         m_DepthMesh = nullptr;       // the pipeline below was built for this mesh + cull mode
        RHI::CullMode       m_DepthCull = RHI::CullMode::Back;
        RHI::IPipeline*     m_DepthPipeline = nullptr;   // owned by the ResourceCache
    };
}
