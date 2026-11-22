#pragma once
#include "EditorContext.h"
#include "ViewportMath.h"
#include "renderer/Core/RenderView.h"
#include <memory>

namespace DXEngine
{
    class Camera;

    // ==================================================================
    // SecondViewPanel: the same scene seen through a SECOND camera, in its own window ("View 2" in the View menu).
    //
    // It owns a RenderView (its own texture, sized to the panel) and a Camera of its own, and draws the editor's scene
    // through them while the panel is visible. The camera starts at the pose the editor camera has when the panel opens;
    // "Pin to editor camera" copies the editor camera's pose again, so you can park a view, fly somewhere else, and compare
    // (the same light from two sides, a shadow from far away, an object from behind). It only looks: no picking, no gizmo.
    //
    // Why it renders inside the UI pass: the panel is the only one who knows the size of its picture and the only one who
    // shows it. The scene is drawn into the view's texture right before the ImGui::Image that shows it, so the texture
    // is never replaced while an earlier part of the same frame still points at it. Closing the panel frees the texture.
    // ==================================================================
    class SecondViewPanel
    {
    public:
        SecondViewPanel();
        ~SecondViewPanel();
        SecondViewPanel(const SecondViewPanel&) = delete;
        SecondViewPanel& operator=(const SecondViewPanel&) = delete;

        void OnImGui(EditorContext& ctx, bool* open);

        // This view's camera takes the editor camera's pose (position and rotation), now.
        void Pin(EditorContext& ctx);

        bool HasView() const { return m_View != nullptr; }
        const Camera* ViewCamera() const { return m_Camera.get(); }
        ViewportMath::Size TargetSize() const { return m_Size; }

    private:
        std::unique_ptr<RenderView> m_View;
        std::shared_ptr<Camera> m_Camera;
        ViewportMath::Size m_Size{ 1, 1 };
    };
}
