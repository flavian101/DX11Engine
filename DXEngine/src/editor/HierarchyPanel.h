#pragma once
#include "EditorContext.h"
#include "HierarchyModel.h"

namespace DXEngine
{
    // The "Hierarchy" window: the scene's entities as a tree.
    //   click          select                      double click   expand / collapse
    //   arrow          expand / collapse           F2 or slow second click   rename in place (Enter keeps, Esc cancels)
    //   eye            show / hide a mesh          drag onto another entity   make it a child (it stays where it is)
    //   drag to the empty area   unparent          right click   create child, rename, duplicate, unparent, delete
    //   Delete, Ctrl+D (window focused)            search box: a flat list of the entities whose name matches
    class HierarchyPanel
    {
    public:
        void OnImGui(EditorContext& ctx, bool* open);

        void BeginRename(Entity e);

    private:
        void DrawRow(EditorContext& ctx, const HierarchyRow& row);
        void ApplyPending(EditorContext& ctx);

        HierarchyModel m_Model;
        char m_Filter[96] = {};
        bool m_ScrollToSelection = false;

        // in-place rename
        Entity m_RenameEntity;
        char   m_RenameBuffer[128] = {};
        bool   m_RenameFocus = false;

        // Structural changes are collected while the rows are drawn and applied after: changing the scene in the middle of
        // the loop would invalidate the rows being walked.
        Entity m_PendingDelete;
        Entity m_PendingDuplicate;
        Entity m_PendingReparentChild;
        Entity m_PendingReparentParent;
        bool   m_HasPendingReparent = false;
    };
}
