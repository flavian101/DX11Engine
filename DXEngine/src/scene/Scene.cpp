#include "dxpch.h"
#include "Scene.h"
#include <algorithm>
#include <unordered_map>

namespace DXEngine
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        const entt::entity id = m_Registry.create();
        m_Registry.emplace<NameComponent>(id, NameComponent{ name });
        m_Registry.emplace<TransformComponent>(id);
        return Entity(id, this);
    }

    Entity Scene::CreateEntity(const std::string& name, Entity parent)
    {
        Entity entity = CreateEntity(name);
        if (parent.IsValid())
            SetParent(entity, parent);
        return entity;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity.IsValid() || entity.GetScene() != this) return;

        // ONE pass over the hierarchy builds parent -> children; the subtree is then walked breadth-first. (The old
        // version scanned every entity for every node of the subtree: O(entities x descendants), and recursive.)
        std::unordered_map<entt::entity, std::vector<entt::entity>> childrenOf;
        for (auto [e, p] : m_Registry.view<ParentComponent>().each())
            childrenOf[p.parent].push_back(e);

        std::vector<entt::entity> doomed{ entity.Id() };
        for (size_t i = 0; i < doomed.size(); ++i)
        {
            const auto it = childrenOf.find(doomed[i]);
            if (it != childrenOf.end())
                doomed.insert(doomed.end(), it->second.begin(), it->second.end());
        }
        for (entt::entity e : doomed)
            if (m_Registry.valid(e))
                m_Registry.destroy(e);
    }

    bool Scene::SetParent(Entity child, Entity parent)
    {
        if (!child.IsValid() || child.GetScene() != this) return false;

        if (!parent.IsValid())                       // unparent
        {
            m_Registry.remove<ParentComponent>(child.Id());
            return true;
        }
        if (parent.GetScene() != this) return false;

        // a cycle would appear if the new parent is the child or lives below it
        for (entt::entity e = parent.Id(); e != entt::null;)
        {
            if (e == child.Id()) return false;
            const ParentComponent* up = m_Registry.try_get<ParentComponent>(e);
            e = up ? up->parent : entt::null;
        }

        m_Registry.emplace_or_replace<ParentComponent>(child.Id(), ParentComponent{ parent.Id() });
        return true;
    }

    Entity Scene::GetParent(Entity entity)
    {
        if (const ParentComponent* p = entity.TryGet<ParentComponent>())
            return Wrap(p->parent);
        return Entity();
    }

    std::vector<Entity> Scene::GetChildren(Entity entity)
    {
        std::vector<Entity> children;
        if (!entity.IsValid()) return children;
        for (auto [e, p] : m_Registry.view<ParentComponent>().each())
            if (p.parent == entity.Id())
                children.push_back(Wrap(e));
        return children;
    }

    Entity Scene::FindByName(const std::string& name)
    {
        for (auto [e, n] : m_Registry.view<NameComponent>().each())
            if (n.name == name)
                return Wrap(e);
        return Entity();
    }
}
