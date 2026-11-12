#pragma once
#include "scene/Scene.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace DXEngine
{
    // One line of the hierarchy panel.
    struct HierarchyRow
    {
        Entity entity;
        int    depth = 0;
        bool   hasChildren = false;
        bool   expanded = false;
    };

    // ==================================================================
    // HierarchyModel: the scene's parent / child tree flattened into the rows that are VISIBLE (a collapsed entity hides
    // its subtree). Rebuilt every frame in O(entities): one pass builds a child list per parent, so a scene with a thousand
    // roots (the stress lights) costs a thousand rows and nothing more.
    //
    // Rows are flat so the panel can clip them (ImGuiListClipper) and draw only what is on screen.
    // Order is by entity id: stable (it does not change when an unrelated entity is destroyed).
    // The expanded / collapsed state lives here, keyed by entity; entries of destroyed entities are dropped on rebuild.
    // ==================================================================
    class HierarchyModel
    {
    public:
        // With a non-empty 'filter' the tree is replaced by the flat list of entities whose name contains it (case-insensitive).
        void Rebuild(Scene& scene, const std::string& filter = std::string());

        const std::vector<HierarchyRow>& Rows() const { return m_Rows; }
        bool IsFiltering() const { return m_Filtering; }

        bool IsExpanded(Entity e) const { return m_Expanded.count(e.Id()) != 0; }
        void SetExpanded(Entity e, bool expanded);
        void Toggle(Entity e) { SetExpanded(e, !IsExpanded(e)); }

        // Expands every ancestor of 'e', so its row exists after the next Rebuild (used when a pick in the viewport selects it).
        void RevealEntity(Scene& scene, Entity e);

    private:
        std::vector<HierarchyRow> m_Rows;
        std::unordered_set<entt::entity> m_Expanded;
        bool m_Filtering = false;

        // scratch, kept between frames so a steady scene does not reallocate
        std::unordered_map<entt::entity, std::vector<entt::entity>> m_Children;
        std::vector<entt::entity> m_Roots;
        std::vector<std::pair<entt::entity, int>> m_Stack;
    };

    // case-insensitive "text contains needle" (ASCII); an empty needle matches everything
    bool ContainsNoCase(const std::string& text, const std::string& needle);
}
