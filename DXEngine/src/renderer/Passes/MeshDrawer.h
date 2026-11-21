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
    class ShaderProgram;
    namespace RHI { class IBuffer; class IPipeline; }

    // ==================================================================
    // MeshDrawer: "how ONE mesh gets drawn". Used by ForwardPass (x3), DepthPrepass and ShadowPass.
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
    // vertex shader, with the light's view-projection standing in for the camera. And the depth PRE-pass (BeginPrepass /
    // DrawPrepass): depth only with the camera, through the same vertex shader the colour pass uses.
    // ==================================================================
    class MeshDrawer
    {
    public:
        bool Initialize(FrameContext& ctx);          // create the shared constant buffers
        void BeginPass(FrameContext& ctx);           // per-pass state: camera matrices, sampler, lights

        void Draw(FrameContext& ctx, const DrawItem& item);                       // 3D item
        // 'afterPrepass': the depth pre-pass already drew this surface (see GetPipeline's flag)
        void DrawMesh(FrameContext& ctx, Mesh& mesh, Material& material,
                      uint32_t submeshIndex, bool afterPrepass = false);          // pipeline+material+geometry only

        // ---- depth pre-pass: the opaque items once, depth only, through the colour pass's own vertex shaders ----
        // BeginPrepass binds what those vertex shaders read (b0 with the CAMERA's matrices, b1 bones): no material, no light.
        // DrawPrepass returns true when the item was drawn; the pass then marks it, and only then does the colour pass
        // rely on the depth buffer for that item.
        void BeginPrepass(FrameContext& ctx);
        bool DrawPrepass(FrameContext& ctx, const DrawItem& item);

        // ---- shadow pass: depth-only drawing ----
        // BeginDepthPass binds only what the caster shader reads (b0, b1). depthBias / slopeBias are the rasterizer's.
        // SetDepthViewProj changes the "camera" between views: it is the light's world -> clip matrix.
        void BeginDepthPass(FrameContext& ctx, int32_t depthBias, float slopeBias);
        void SetDepthViewProj(const DirectX::XMFLOAT4X4& lightViewProj);
        bool DrawDepth(FrameContext& ctx, const DrawItem& item);                  // true when something was drawn

    private:
        void UploadTransform(const DirectX::XMFLOAT4X4& world);
        void UploadBones(const DrawItem& item);
        // The bones an item is drawn with: its pose, or (a mesh that has blend attributes but no pose: no Animator above it,
        // or none published yet) identity matrices. Such a mesh still runs the skinning shader; without this it would read
        // whatever the last character left in b1.
        void UploadBonesFor(const DrawItem& item);
        void BindMaterial(FrameContext& ctx, Material& material);
        void IssueDraw(FrameContext& ctx, Mesh& mesh, uint32_t submeshIndex);
        // bones, then the item's transform(s) and the draw(s): what both depth-only paths do after the pipeline is set
        void SubmitGeometry(FrameContext& ctx, const DrawItem& item);
        std::shared_ptr<ShaderProgram> ResolveProgram(FrameContext& ctx, Mesh& mesh, Material& material);
        RHI::IPipeline* DepthPipelineFor(FrameContext& ctx, Mesh& mesh, RHI::CullMode cull);
        RHI::IPipeline* PrepassPipelineFor(FrameContext& ctx, Mesh& mesh, Material& material);

        std::shared_ptr<RHI::IBuffer> m_TransformCB;
        std::shared_ptr<RHI::IBuffer> m_BoneCB;
        bool m_BonesAreIdentity = false;                  // b1 currently holds identity matrices (skip re-uploading them)

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

        // what the draws of this pass are counted as: scene draws, shadow caster draws or depth pre-pass draws
        enum class DrawKind { Scene, ShadowCaster, Prepass };
        DrawKind            m_Kind = DrawKind::Scene;

        // depth-only drawing (shadow pass)
        int32_t             m_DepthBias = 0;
        float               m_SlopeBias = 0.0f;
        const Mesh*         m_DepthMesh = nullptr;       // the pipeline below was built for this mesh + cull mode
        RHI::CullMode       m_DepthCull = RHI::CullMode::Back;
        RHI::IPipeline*     m_DepthPipeline = nullptr;   // owned by the ResourceCache

        // the depth pre-pass pipeline of the last (material, mesh): items arrive sorted by exactly that, so the shader
        // lookup (a string key) happens once per run of equal items, not once per draw
        const Material*     m_PrepassMaterial = nullptr;
        const Mesh*         m_PrepassMesh = nullptr;
        RHI::IPipeline*     m_PrepassPipeline = nullptr; // owned by the ResourceCache
    };
}
