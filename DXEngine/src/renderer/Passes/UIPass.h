#pragma once
#include "../Core/RenderPass.h"
#include "MeshDrawer.h"

namespace DXEngine
{
    class Mesh;
    class Material;
    namespace RHI { class IBuffer; }

    // Draws 2D UI elements as coloured quads on top of the finished 3D image.
    // Reads:  ctx.scene.uiElements, ctx.targets.color.     Writes: ctx.stats.uiElementsRendered.
    // The orthographic projection is derived from the back buffer size EVERY frame,
    // so it can never go stale after a resize (no UpdateUIProjectionMatrix state needed).
    class UIPass final : public RenderPass
    {
    public:
        bool Initialize(FrameContext& ctx) override;
        void Execute(FrameContext& ctx) override;
        std::string GetName() const override { return "UIPass"; }

    private:
        MeshDrawer                    m_Drawer;
        std::shared_ptr<Mesh>         m_Quad;       // CENTRED unit quad (-0.5 .. 0.5)
        std::shared_ptr<Material>     m_Material;
        std::shared_ptr<RHI::IBuffer> m_UIConstants;
    };
}
