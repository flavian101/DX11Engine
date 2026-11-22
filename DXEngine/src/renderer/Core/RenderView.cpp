#include "dxpch.h"
#include "RenderView.h"
#include "renderer/Renderer.h"
#include "RHI/GraphicsDevice.h"

namespace DXEngine
{
    RenderView::RenderView(Renderer* owner, std::string name)
        : m_Owner(owner), m_Name(std::move(name))
    {
    }

    RenderView::~RenderView()
    {
        if (m_Owner)
            m_Owner->UnregisterView(this);
        ReleaseTargets();
    }

    void RenderView::ReleaseTargets()
    {
        m_Color.reset();
        m_Depth.reset();
        m_Size = {};
    }

    void RenderView::ApplySize(RHI::IGraphicsDevice& device)
    {
        const SceneTargetSize want = ResolveSceneTargetSize(m_RequestWidth, m_RequestHeight,
                                                            device.GetCapabilities().maxTextureSize);
        if (want == m_Size && (m_Color != nullptr) == want.Enabled())
            return;

        ReleaseTargets();
        if (!want.Enabled())
            return;

        RHI::TextureDesc color;
        color.width  = want.width;
        color.height = want.height;
        color.format = RHI::TextureFormat::RGBA8_UNORM;        // the same as the back buffer: pipelines do not care which one they draw to
        color.isRenderTarget = true;
        color.debugName = m_Name + ".Color";

        RHI::TextureDesc depth = color;
        depth.format = RHI::TextureFormat::D24_UNORM_S8_UINT;  // the same as the device's depth buffer
        depth.isRenderTarget = false;
        depth.isDepthStencil = true;
        depth.debugName = m_Name + ".Depth";

        try
        {
            m_Color = device.CreateTexture(color);
            m_Depth = device.CreateTexture(depth);
            m_Size  = want;
        }
        catch (const std::exception& e)       // D3D11Texture reports failure by throwing
        {
            OutputDebugStringA(("WARNING: the texture of view '" + m_Name + "' could not be created, drawing into the back buffer: " + e.what() + "\n").c_str());
            ReleaseTargets();
            m_RequestWidth = m_RequestHeight = 0;   // do not retry (and log) every frame
        }
    }
}
