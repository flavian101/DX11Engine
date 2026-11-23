#include "dxpch.h"
#include "Scene.h"
#include <algorithm>

namespace DXEngine
{
    Entity Scene::CreateEntity(const std::string& name)
    {
        Uuid uuid = Uuid::Generate();
        while (m_ByUuid.count(uuid))               // 64 random bits: practically never, but never store a clash
            uuid = Uuid::Generate();
        return CreateEntityWithUuid(name, uuid);
    }

    Entity Scene::CreateEntityWithUuid(const std::string& name, Uuid uuid)
    {
        if (uuid.IsNull() || m_ByUuid.count(uuid)) return Entity();
        const entt::entity id = m_Registry.create();
        m_Registry.emplace<IdComponent>(id, IdComponent{ uuid });
        m_Registry.emplace<NameComponent>(id, NameComponent{ name });
        m_Registry.emplace<TransformComponent>(id);
        m_ByUuid.emplace(uuid, id);
        return Entity(id, this);
    }

    Entity Scene::FindByUuid(Uuid uuid)
    {
        const auto it = m_ByUuid.find(uuid);
        return (it != m_ByUuid.end() && m_Registry.valid(it->second)) ? Entity(it->second, this) : Entity();
    }

    Uuid Scene::GetUuid(Entity entity) const
    {
        const IdComponent* id = entity.TryGet<IdComponent>();
        return id ? id->id : Uuid{};
    }

    void Scene::Clear()
    {
        m_Registry.clear();
        m_ByUuid.clear();
    }

    Entity Scene::CreateEntity(const std::string& name, Entity parent)
    {
        Entity entity = CreateEntity(name);
        if (parent.IsValid())
            SetParent(entity, parent);
        return entity;
    }

    // ---------------- hierarchy: the three edits everything else is made of ----------------
    // Invariant (RelationshipComponent): a parent's child list runs firstChild -> nextSibling -> ... -> lastChild, every
    // child points back with parent / prevSibling, and an entity that has neither a parent nor children has no component.

    void Scene::DropIfEmpty(entt::entity entity)
    {
        const RelationshipComponent* r = m_Registry.valid(entity) ? m_Registry.try_get<RelationshipComponent>(entity) : nullptr;
        if (r && r->parent == entt::null && r->firstChild == entt::null)
            m_Registry.remove<RelationshipComponent>(entity);
    }

    void Scene::Detach(entt::entity child)
    {
        RelationshipComponent* rel = m_Registry.try_get<RelationshipComponent>(child);
        if (!rel || rel->parent == entt::null) return;

        if (TransformComponent* t = m_Registry.try_get<TransformComponent>(child))
            t->MarkDirty();                                  // its world matrix no longer includes the same parent

        const entt::entity parent = rel->parent, prev = rel->prevSibling, next = rel->nextSibling;
        rel->parent = rel->prevSibling = rel->nextSibling = entt::null;

        // Neighbours that are gone (destroyed behind our back) are skipped rather than dereferenced.
        const auto link = [this](entt::entity e) { return m_Registry.valid(e) ? m_Registry.try_get<RelationshipComponent>(e) : nullptr; };
        if (RelationshipComponent* p = link(prev))   p->nextSibling = next;
        else if (RelationshipComponent* up = link(parent)) up->firstChild = next;
        if (RelationshipComponent* n = link(next))   n->prevSibling = prev;
        else if (RelationshipComponent* up = link(parent)) up->lastChild = prev;

        DropIfEmpty(child);
        DropIfEmpty(parent);
    }

    void Scene::Attach(entt::entity child, entt::entity parent)
    {
        // Add both components BEFORE taking references: adding one can move the others.
        m_Registry.get_or_emplace<RelationshipComponent>(parent);
        m_Registry.get_or_emplace<RelationshipComponent>(child);
        RelationshipComponent& p = m_Registry.get<RelationshipComponent>(parent);
        RelationshipComponent& c = m_Registry.get<RelationshipComponent>(child);
        if (TransformComponent* t = m_Registry.try_get<TransformComponent>(child))
            t->MarkDirty();                                  // new parent, new world matrix

        c.parent      = parent;
        c.prevSibling = p.lastChild;
        c.nextSibling = entt::null;
        if (p.lastChild != entt::null) m_Registry.get<RelationshipComponent>(p.lastChild).nextSibling = child;
        else                           p.firstChild = child;
        p.lastChild = child;
    }

    void Scene::DestroyEntity(Entity entity)
    {
        if (!entity.IsValid() || entity.GetScene() != this) return;

        // Cut the subtree out of the hierarchy first, so the parent never points at a destroyed entity.
        Detach(entity.Id());

        // The subtree, breadth first, by following the child links: its size is the cost, no other entity is looked at.
        std::vector<entt::entity> doomed{ entity.Id() };
        for (size_t i = 0; i < doomed.size(); ++i)
        {
            const RelationshipComponent* rel = m_Registry.try_get<RelationshipComponent>(doomed[i]);
            for (entt::entity c = rel ? rel->firstChild : entt::null; c != entt::null && m_Registry.valid(c); )
            {
                doomed.push_back(c);
                const RelationshipComponent* childRel = m_Registry.try_get<RelationshipComponent>(c);
                c = childRel ? childRel->nextSibling : entt::null;
            }
        }
        for (entt::entity e : doomed)
            if (m_Registry.valid(e))
            {
                if (const IdComponent* id = m_Registry.try_get<IdComponent>(e))
                    m_ByUuid.erase(id->id);
                m_Registry.destroy(e);
            }
    }

    bool Scene::SetParent(Entity child, Entity parent)
    {
        if (!child.IsValid() || child.GetScene() != this) return false;

        if (!parent.IsValid())                       // unparent
        {
            Detach(child.Id());
            return true;
        }
        if (parent.GetScene() != this) return false;

        // a cycle would appear if the new parent is the child or lives below it
        for (entt::entity e = parent.Id(); e != entt::null;)
        {
            if (e == child.Id()) return false;
            const RelationshipComponent* up = m_Registry.try_get<RelationshipComponent>(e);
            e = up ? up->parent : entt::null;
        }

        const RelationshipComponent* current = m_Registry.try_get<RelationshipComponent>(child.Id());
        if (current && current->parent == parent.Id())
            return true;                             // already its parent: nothing moves, the order of the children stays

        Detach(child.Id());
        Attach(child.Id(), parent.Id());
        return true;
    }

    Entity Scene::GetParent(Entity entity)
    {
        if (const RelationshipComponent* r = entity.TryGet<RelationshipComponent>())
            if (r->parent != entt::null)
                return Wrap(r->parent);
        return Entity();
    }

    Entity Scene::FirstChild(Entity entity)
    {
        if (const RelationshipComponent* r = entity.TryGet<RelationshipComponent>())
            if (r->firstChild != entt::null)
                return Wrap(r->firstChild);
        return Entity();
    }

    Entity Scene::NextSibling(Entity entity)
    {
        if (const RelationshipComponent* r = entity.TryGet<RelationshipComponent>())
            if (r->nextSibling != entt::null)
                return Wrap(r->nextSibling);
        return Entity();
    }

    std::vector<Entity> Scene::GetChildren(Entity entity)
    {
        std::vector<Entity> children;
        for (Entity c = FirstChild(entity); c; c = NextSibling(c))
            children.push_back(c);
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
