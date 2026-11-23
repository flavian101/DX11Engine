#pragma once
#include "Components.h"
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <unordered_map>
#include <vector>

namespace DXEngine
{
    class Scene;

    // ==================================================================
    // Entity: a small VALUE handle (id + scene). It is safe to keep and to copy:
    // after the entity is destroyed IsValid() is false and TryGet() returns nullptr, instead of
    // reading freed memory (EnTT recycles ids with a generation counter, so a stale handle never
    // silently points at a newer entity).
    //
    //   if (auto* t = entity.TryGet<TransformComponent>()) { ... }   // fallible access: pointer
    //   entity.Get<TransformComponent>().position.x = 3;             // must exist: throws a clear error
    // ==================================================================
    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity id, Scene* scene) : m_Id(id), m_Scene(scene) {}

        bool IsValid() const;
        explicit operator bool() const { return IsValid(); }

        // adds or replaces. Returns the component (void for empty tag components such as SkyboxComponent).
        template<typename T, typename... Args> decltype(auto) Add(Args&&... args);
        template<typename T> bool Has() const;
        template<typename T> T* TryGet();
        template<typename T> const T* TryGet() const;
        template<typename T> T& Get();            // throws std::runtime_error if invalid or missing
        template<typename T> void Remove();

        entt::entity Id() const { return m_Id; }
        Scene* GetScene() const { return m_Scene; }

        bool operator==(const Entity& o) const { return m_Id == o.m_Id && m_Scene == o.m_Scene; }
        bool operator!=(const Entity& o) const { return !(*this == o); }

    private:
        entt::entity m_Id = entt::null;
        Scene* m_Scene = nullptr;
    };

    // ==================================================================
    // Scene: the world. Owns every entity and component. Systems (transform, animation, rendering
    // extraction, ...) are free functions/classes that read and write components of a Scene.
    // ==================================================================
    class Scene
    {
    public:
        // Every entity starts with an Id (a fresh Uuid), a Name and a Transform (position 0, no rotation, scale 1).
        Entity CreateEntity(const std::string& name = "Entity");
        Entity CreateEntity(const std::string& name, Entity parent);

        // For loading: an entity that has THIS id. An invalid Entity (nothing created) when the id is null or already used.
        Entity CreateEntityWithUuid(const std::string& name, Uuid id);

        // Identity. FindByUuid is O(1); an id nobody has (or a destroyed entity's) gives an invalid Entity.
        Entity FindByUuid(Uuid id);
        Uuid   GetUuid(Entity entity) const;

        // Destroys every entity (and so forgets every id). Handles to the old entities are invalid afterwards.
        void Clear();

        // Destroys the entity AND all its descendants, and removes it from its parent's children. Cost: the size of
        // the subtree (the hierarchy is linked, see RelationshipComponent; nothing scans all entities).
        void DestroyEntity(Entity entity);

        // parent invalid (default Entity()) = make it a root. Returns false (and changes nothing) if the
        // parent is the entity itself or one of its descendants, which would make a cycle. The child becomes the LAST
        // child of its new parent; setting the parent it already has changes nothing (the order stays).
        // Cost: the depth of the new parent (the cycle check), nothing else.
        // The local transform is kept (the entity moves with the new parent): see SceneEditing::Reparent for the other kind.
        bool SetParent(Entity child, Entity parent);
        Entity GetParent(Entity entity);

        // The children in the order they were attached. GetChildren allocates a vector: to walk them without that, use
        //   for (Entity c = scene.FirstChild(e); c; c = scene.NextSibling(c)) { ... }
        // (do not change the hierarchy of the entity you are standing on while walking: take NextSibling first).
        std::vector<Entity> GetChildren(Entity entity);
        Entity FirstChild(Entity entity);
        Entity NextSibling(Entity entity);

        Entity FindByName(const std::string& name);

        size_t EntityCount() const
        {
            const auto* storage = m_Registry.storage<entt::entity>();   // const access returns a pointer (null before the first create)
            return storage ? storage->free_list() : 0;   // live entities occupy the first free_list() slots (EnTT 3.14)
        }

        // direct access for systems
        entt::registry& Registry() { return m_Registry; }
        const entt::registry& Registry() const { return m_Registry; }

        Entity Wrap(entt::entity id) { return Entity(id, this); }

    private:
        // The three hierarchy edits SetParent and DestroyEntity are made of. They keep RelationshipComponent consistent.
        void Detach(entt::entity child);                        // unlink from the parent's child list; the child becomes a root
        void Attach(entt::entity child, entt::entity parent);   // append to the parent's child list (the child must be a root)
        void DropIfEmpty(entt::entity entity);                  // remove the RelationshipComponent when it holds nothing

        entt::registry m_Registry;
        std::unordered_map<Uuid, entt::entity> m_ByUuid;       // IdComponent -> entity; kept by CreateEntity / DestroyEntity / Clear
    };

    // ---------------- Entity templates ----------------
    inline bool Entity::IsValid() const
    {
        return m_Scene != nullptr && m_Id != entt::null && m_Scene->Registry().valid(m_Id);
    }

    template<typename T, typename... Args>
    decltype(auto) Entity::Add(Args&&... args)
    {
        if (!IsValid()) throw std::runtime_error("Entity::Add on an invalid entity");
        return m_Scene->Registry().template emplace_or_replace<T>(m_Id, std::forward<Args>(args)...);
    }

    template<typename T>
    bool Entity::Has() const
    {
        return IsValid() && m_Scene->Registry().template all_of<T>(m_Id);
    }

    template<typename T>
    T* Entity::TryGet()
    {
        return IsValid() ? m_Scene->Registry().template try_get<T>(m_Id) : nullptr;
    }

    template<typename T>
    const T* Entity::TryGet() const
    {
        return IsValid() ? static_cast<const Scene*>(m_Scene)->Registry().template try_get<T>(m_Id) : nullptr;
    }

    template<typename T>
    T& Entity::Get()
    {
        T* component = TryGet<T>();
        if (!component)
            throw std::runtime_error(std::string("Entity::Get: ") + (IsValid() ? "entity has no " : "invalid entity, wanted ") + typeid(T).name());
        return *component;
    }

    template<typename T>
    void Entity::Remove()
    {
        if (IsValid()) m_Scene->Registry().template remove<T>(m_Id);
    }
}
