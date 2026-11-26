#pragma once
#include "scene/Scene.h"
#include <algorithm>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // EditorSelection: which entities the editor panels are about.
    //
    // An ordered set. The last one selected is the PRIMARY (the "active" one): the Inspector shows it, the gizmo sits on it, a
    // shift-click range starts from it. Get() answers the primary, so code that deals with one entity keeps working unchanged.
    //
    // It stores Entity VALUES, so a destroyed entity cannot be read through it: it drops out of All(), Count() and Get() by itself
    // (and the primary becomes the one selected before it).
    // ==================================================================
    class EditorSelection
    {
    public:
        // the primary: the entity selected last, or an invalid Entity when nothing is selected (or all of it was destroyed)
        Entity Get() const
        {
            for (size_t i = m_Entities.size(); i-- > 0;)
                if (m_Entities[i].IsValid()) return m_Entities[i];
            return Entity();
        }
        bool   Has() const { return Get().IsValid(); }
        // 'e' is selected (the primary or not)
        bool   Is(Entity e) const { return e.IsValid() && std::find(m_Entities.begin(), m_Entities.end(), e) != m_Entities.end(); }
        bool   IsPrimary(Entity e) const { return e.IsValid() && e == Get(); }
        size_t Count() const { return All().size(); }

        // the selected entities that still exist, in the order they were selected (the primary is the last)
        std::vector<Entity> All() const
        {
            std::vector<Entity> out;
            out.reserve(m_Entities.size());
            for (const Entity& e : m_Entities)
                if (e.IsValid()) out.push_back(e);
            return out;
        }

        // only 'e' (an invalid entity: nothing)
        void Set(Entity e)
        {
            m_Entities.clear();
            if (e.IsValid()) m_Entities.push_back(e);
        }

        // exactly these, in this order (the last is the primary); invalid ones and repeats are left out
        void SetMany(const std::vector<Entity>& entities)
        {
            m_Entities.clear();
            for (const Entity& e : entities) Add(e);
        }

        // 'e' joins and becomes the primary (selected already: it only becomes the primary)
        void Add(Entity e)
        {
            if (!e.IsValid()) return;
            Prune();
            m_Entities.erase(std::remove(m_Entities.begin(), m_Entities.end(), e), m_Entities.end());
            m_Entities.push_back(e);
        }

        void Remove(Entity e)
        {
            m_Entities.erase(std::remove(m_Entities.begin(), m_Entities.end(), e), m_Entities.end());
        }

        // selected: leaves; not selected: joins as the primary
        void Toggle(Entity e)
        {
            if (!e.IsValid()) return;
            if (Is(e)) Remove(e); else Add(e);
        }

        void Clear() { m_Entities.clear(); }

    private:
        void Prune() { m_Entities.erase(std::remove_if(m_Entities.begin(), m_Entities.end(), [](const Entity& e) { return !e.IsValid(); }), m_Entities.end()); }

        std::vector<Entity> m_Entities;
    };
}
