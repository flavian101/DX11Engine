#pragma once
#include "Components.h"
#include <stdexcept>
#include <string>
#include <typeinfo>
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

        template<typename T, typename... Args> T& Add(Args&&... args);   // adds or replaces
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
        // Every entity starts with a Name and a Transform (position 0, no rotation, scale 1).
        Entity CreateEntity(const std::string& name = "Entity");
        Entity CreateEntity(const std::string& name, Entity parent);

        // Destroys the entity AND all its descendants.
        void DestroyEntity(Entity entity);

        // parent invalid (default Entity()) = make it a root. Returns false (and changes nothing) if the
        // parent is the entity itself or one of its descendants, which would make a cycle.
        bool SetParent(Entity child, Entity parent);
        Entity GetParent(Entity entity);
        std::vector<Entity> GetChildren(Entity entity);

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
        void CollectDescendants(entt::entity root, std::vector<entt::entity>& out);

        entt::registry m_Registry;
    };

    // ---------------- Entity templates ----------------
    inline bool Entity::IsValid() const
    {
        return m_Scene != nullptr && m_Id != entt::null && m_Scene->Registry().valid(m_Id);
    }

    template<typename T, typename... Args>
    T& Entity::Add(Args&&... args)
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
