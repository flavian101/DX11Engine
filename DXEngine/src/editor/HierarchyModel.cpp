#include "dxpch.h"
#include "HierarchyModel.h"
#include <algorithm>
#include <cctype>

namespace DXEngine
{
    bool ContainsNoCase(const std::string& text, const std::string& needle)
    {
        if (needle.empty()) return true;
        auto lower = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
        auto it = std::search(text.begin(), text.end(), needle.begin(), needle.end(),
                              [&](char a, char b) { return lower(static_cast<unsigned char>(a)) == lower(static_cast<unsigned char>(b)); });
        return it != text.end();
    }

    void HierarchyModel::SetExpanded(Entity e, bool expanded)
    {
        if (!e.IsValid()) return;
        if (expanded) m_Expanded.insert(e.Id());
        else          m_Expanded.erase(e.Id());
    }

    void HierarchyModel::RevealEntity(Scene& scene, Entity e)
    {
        for (Entity parent = scene.GetParent(e); parent.IsValid(); parent = scene.GetParent(parent))
            m_Expanded.insert(parent.Id());
    }

    void HierarchyModel::Rebuild(Scene& scene, const std::string& filter)
    {
        entt::registry& reg = scene.Registry();
        m_Rows.clear();
        m_Filtering = !filter.empty();

        // forget the state of entities that no longer exist
        for (auto it = m_Expanded.begin(); it != m_Expanded.end();)
            it = reg.valid(*it) ? std::next(it) : m_Expanded.erase(it);

        // every scene entity has a transform (Scene::CreateEntity adds it)
        const auto byId = [](entt::entity a, entt::entity b) { return entt::to_integral(a) < entt::to_integral(b); };

        if (m_Filtering)
        {
            m_Roots.clear();
            for (auto [e, name] : reg.view<NameComponent>().each())
                if (ContainsNoCase(name.name, filter))
                    m_Roots.push_back(e);
            std::sort(m_Roots.begin(), m_Roots.end(), byId);
            for (entt::entity e : m_Roots)
                m_Rows.push_back({ scene.Wrap(e), 0, false, false });
            return;
        }

        for (auto& entry : m_Children) entry.second.clear();     // keep the vectors' capacity
        m_Roots.clear();

        for (auto [e, p] : reg.view<ParentComponent>().each())
            if (reg.valid(p.parent)) m_Children[p.parent].push_back(e);

        for (auto [e, transform] : reg.view<TransformComponent>().each())
        {
            (void)transform;
            const ParentComponent* up = reg.try_get<ParentComponent>(e);
            if (!up || !reg.valid(up->parent))          // a root (or an orphan whose parent is gone)
                m_Roots.push_back(e);
        }

        std::sort(m_Roots.begin(), m_Roots.end(), byId);
        for (auto& entry : m_Children) std::sort(entry.second.begin(), entry.second.end(), byId);

        // depth-first, children right after their parent. Roots are pushed in reverse so the first one is popped first.
        m_Stack.clear();
        for (auto it = m_Roots.rbegin(); it != m_Roots.rend(); ++it) m_Stack.push_back({ *it, 0 });

        while (!m_Stack.empty())
        {
            const auto [e, depth] = m_Stack.back();
            m_Stack.pop_back();

            const auto kids = m_Children.find(e);
            const bool hasChildren = kids != m_Children.end() && !kids->second.empty();
            const bool expanded = hasChildren && m_Expanded.count(e) != 0;
            m_Rows.push_back({ scene.Wrap(e), depth, hasChildren, expanded });

            if (expanded)
                for (auto it = kids->second.rbegin(); it != kids->second.rend(); ++it)
                    m_Stack.push_back({ *it, depth + 1 });
        }
    }
}
