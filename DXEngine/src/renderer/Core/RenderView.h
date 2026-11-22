#pragma once
#include "RenderTypes.h"
#include "SceneTarget.h"
#include <memory>
#include <string>

namespace DXEngine
{
    class Renderer;
    namespace RHI { class IGraphicsDevice; class ITexture; }

    // ==================================================================
    // RenderView: one way of looking at a scene. It is WHERE a picture is drawn and how, not WHAT is drawn:
    //
    //   target       the window's back buffer (size 0 x 0, the default), or a colour + depth texture pair of the size
    //                asked for with SetSize (what an editor shows inside a panel)
    //   clear colour what the target is cleared to before the scene is drawn
    //   statistics   of the last time something was drawn through it
    //
    // The scene and the camera are arguments of Renderer::Render(view, scene, camera): the same scene can be drawn through
    // any number of views, each with its own camera (an editor viewport and a second one, a material preview).
    //
    // Everything GPU-side that is not a target is SHARED and lives in the Renderer: the device, the caches, the shadow
    // atlas, the light buffers. Views are drawn one after the other (each is recorded and submitted completely before the
    // next one starts), so sharing is safe.
    //
    // Lifetime: the Renderer owns the main view and hands out the others as unique_ptr (CreateView). A view may be
    // destroyed before or after the Renderer: if the Renderer goes first it releases every view's textures, so a view
    // that is destroyed later holds no GPU object.
    // ==================================================================
    class RenderView
    {
    public:
        ~RenderView();
        RenderView(const RenderView&) = delete;
        RenderView& operator=(const RenderView&) = delete;

        const std::string& GetName() const { return m_Name; }

        // Size of the texture to draw into; 0 or less on either side = draw into the window's back buffer. Applied at the
        // start of the next Render, never in the middle of a frame (the UI being built may still point at the old texture).
        // Sizes are clamped to what the device supports (ResolveSceneTargetSize).
        void SetSize(int width, int height) { m_RequestWidth = width; m_RequestHeight = height; }
        int  RequestedWidth() const  { return m_RequestWidth; }
        int  RequestedHeight() const { return m_RequestHeight; }

        // The texture this view draws into, or null while it draws into the back buffer. It is replaced when the size
        // changes, so do not keep the pointer past the frame.
        RHI::ITexture* GetTexture() const { return m_Color.get(); }
        bool DrawsToTexture() const { return m_Color != nullptr; }
        // The size that really exists (0 x 0 while there is no texture)
        SceneTargetSize GetTargetSize() const { return m_Size; }

        void SetClearColor(float r, float g, float b, float a = 1.0f)
        {
            m_Clear[0] = r; m_Clear[1] = g; m_Clear[2] = b; m_Clear[3] = a;
        }
        const float* GetClearColor() const { return m_Clear; }

        // Of the last Render through this view (empty before the first one).
        const RenderStatistics& GetStats() const { return m_Stats; }

    private:
        friend class Renderer;
        RenderView(Renderer* owner, std::string name);

        // Makes the texture pair match the request. Renderer::Render calls it at the start of every draw.
        void ApplySize(RHI::IGraphicsDevice& device);
        void ReleaseTargets();

        Renderer*   m_Owner = nullptr;          // null once the Renderer is gone
        std::string m_Name;

        int m_RequestWidth  = 0;
        int m_RequestHeight = 0;
        SceneTargetSize m_Size;                 // what exists

        std::shared_ptr<RHI::ITexture> m_Color;
        std::shared_ptr<RHI::ITexture> m_Depth;

        float m_Clear[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
        RenderStatistics m_Stats;
    };
}
