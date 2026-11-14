#pragma once
#include "EditorContext.h"
#include "TransformEditing.h"

namespace DXEngine
{
    // The "Inspector" window: the selected entity, component by component, in a label | control grid (PropertyGrid).
    // The header holds the kind icon, the name and "Add component"; every component is a collapsible section (the removable
    // ones have a close button). Materials are shared assets: editing one changes every object that uses it, and the panel says
    // so. Material edits go through Material's setters (they mark the GPU copy as outdated).
    class InspectorPanel
    {
    public:
        void OnImGui(EditorContext& ctx, bool* open);

    private:
        void DrawHeader(EditorContext& ctx, Entity e);
        void DrawTransform(Entity e);
        void DrawMeshRenderer(Entity e);
        void DrawLight(Entity e);

        TransformEditing::EulerCache m_Euler;
        Entity m_LastEntity;               // the entity the Euler cache belongs to
        char   m_NameBuffer[128] = {};
        bool   m_EditingName = false;
        bool   m_UniformScale = false;
        bool   m_RemoveLight = false;
        bool   m_RemoveRenderer = false;
    };
}
