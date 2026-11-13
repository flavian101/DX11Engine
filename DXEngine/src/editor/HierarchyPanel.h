#pragma once
#include "EditorContext.h"
#include "HierarchyModel.h"

namespace DXEngine
{
    // The "Hierarchy" window: the scene's entities as a tree. Click selects, the arrow expands, drag one entity onto another
    // to make it a child (it stays where it is in the world), drop on the empty area to make it a root, right-click for
    // create / duplicate / unparent / delete. Delete and Ctrl+D work while the window has focus. The search box shows a flat
    // list of the entities whose name matches.
    class HierarchyPanel
    {
    public:
        void OnImGui(EditorContext& ctx, bool* open);

    private:
        void DrawRow(EditorContext& ctx, const HierarchyRow& row);
        void ApplyPending(EditorContext& ctx);

        HierarchyModel m_Model;
        char m_Filter[96] = {};
        bool m_ScrollToSelection = false;

        // Structural changes are collected while the rows are drawn and applied after: changing the scene in the middle of
        // the loop would invalidate the rows being walked.
        Entity m_PendingDelete;
        Entity m_PendingDuplicate;
        Entity m_PendingReparentChild;
        Entity m_PendingReparentParent;
        bool   m_HasPendingReparent = false;
    };
}
