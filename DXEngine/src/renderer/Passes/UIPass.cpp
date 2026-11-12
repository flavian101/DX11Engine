#include "dxpch.h"
#include "UIPass.h"
#include "../Cache/ResourceCache.h"
#include "../MaterialSystem.h"
#include "RHI/GraphicsDevice.h"
#include "utils/ConstantBufferTypes.h"
#include "utils/Mesh/Mesh.h"
#include "utils/UI/UIElement.h"
#include "utils/material/Material.h"

namespace DXEngine
{
    using namespace DirectX;

    bool UIPass::Initialize(FrameContext& ctx)
    {
        auto& svc = *ctx.services;

        m_Quad     = Mesh::CreateQuad(svc.device, 1.0f, 1.0f);
        m_Material = svc.materials->GetDefaultMaterial(ShadingModel::UI);
        if (!m_Quad || !m_Material)
        {
            OutputDebugStringA("ERROR: UIPass::Initialize - missing UI quad or default UI material\n");
            return false;
        }

        m_UIConstants = svc.resources->GetOrCreateConstantBuffer("UI.Constants", sizeof(UIConstantBuffer));
        return m_UIConstants && m_Drawer.Initialize(ctx);
    }

    void UIPass::Execute(FrameContext& ctx)
    {
        if (ctx.scene.uiElements.empty() || !ctx.cmd || !ctx.targets.color) return;

        const float w = static_cast<float>(ctx.targets.color->GetWidth());
        const float h = static_cast<float>(ctx.targets.color->GetHeight());

        ctx.cmd->SetRenderTarget(ctx.targets.color, nullptr);     // UI never touches depth
        RHI::ViewportDesc vp; vp.width = w; vp.height = h;
        ctx.cmd->SetViewport(vp);
        m_Drawer.BeginPass(ctx);

        // y grows DOWNWARD (top = 0), matching UIRect coordinates
        const XMMATRIX ortho = XMMatrixOrthographicOffCenterLH(0.0f, w, h, 0.0f, -1.0f, 1.0f);

        for (const auto& element : ctx.scene.uiElements)
        {
            if (!element || !element->IsVisible()) continue;

            const UIRect&  b = element->GetBounds();
            const UIColor  c = element->GetRenderColor();              // element knows its own state colour
            m_Material->SetDiffuseColor({ c.r, c.g, c.b, c.a });

            // the quad is CENTRED, UIRect is top-left based -> shift by half the size
            const XMMATRIX model = XMMatrixScaling(b.width, b.height, 1.0f) *
                                   XMMatrixTranslation(b.x + b.width * 0.5f, b.y + b.height * 0.5f, 0.0f);

            UIConstantBuffer cb;
            cb.projection   = XMMatrixTranspose(model * ortho);
            cb.screenWidth  = w;
            cb.screenHeight = h;
            cb.time         = ctx.totalTime;
            cb.padding      = 0.0f;
            m_UIConstants->Update(&cb, sizeof(cb), 0);
            ctx.cmd->SetConstantBuffer(m_UIConstants.get(), CB_UI, RHI::ShaderStage::Vertex);

            m_Drawer.InvalidateMaterialCache();        // colour just changed -> force material re-upload
            m_Drawer.DrawMesh(ctx, *m_Quad, *m_Material, 0);
            ++ctx.stats.uiElementsRendered;
        }
    }
}
