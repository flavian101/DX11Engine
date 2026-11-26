#pragma once
#include "EditorContext.h"
#include "HierarchyModel.h"

namespace DXEngine
{
    // The "Hierarchy" window: the scene's entities as a tree.
    //   click          select                      double click   expand / collapse
    //   Ctrl+click     add to / remove from the selection      Shift+click   select the rows from the last click to this one
    //   (every command below acts on the whole selection: Delete, Ctrl+D, Unparent, and a drag of any selected row)
    //   arrow          expand / collapse           F2 or slow second click   rename in place (Enter keeps, Esc cancels)
    //   eye            show / hide a mesh          drag onto another entity   make it a child (it stays where it is)
    //   drag to the empty area   unparent          right click   create child, rename, duplicate, unparent, delete
    //   Delete, Ctrl+D (window focused)            search box: a flat list of the entities whose name matches
    class HierarchyPanel
    {
    public:
        void OnImGui(EditorContext& ctx, bool* open);

        void BeginRename(Entity e);

        HierarchyModel& Model() { return m_Model; }        // the expanded / collapsed state, for the host to carry over a restored scene

    private:
        void DrawRow(EditorContext& ctx, const HierarchyRow& row);
        void ApplyPending(EditorContext& ctx);
        void SelectByClick(EditorContext& ctx, Entity clicked);

        HierarchyModel m_Model;
        char m_Filter[96] = {};
        bool m_ScrollToSelection = false;

        // in-place rename
        Entity m_RenameEntity;
        char   m_RenameBuffer[128] = {};
        bool   m_RenameFocus = false;

        // Structural changes are collected while the rows are drawn and applied after: changing the scene in the middle of
        // the loop would invalidate the rows being walked.
        bool   m_PendingDelete = false;                // delete / duplicate: the selection
        bool   m_PendingDuplicate = false;
        std::vector<Entity> m_PendingReparentChildren; // what a drag carried (a selected row carries the whole selection) ...
        Entity m_PendingReparentParent;                // ... and where it was dropped (invalid = the root level)
        bool   m_HasPendingReparent = false;

        Entity m_Anchor;                               // where a Shift+click range starts: the last entity selected without Shift
    };
}
