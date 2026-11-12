#pragma once
#include "scene/Scene.h"

namespace DXEngine
{
    // ==================================================================
    // EditorSelection: which entity the editor panels are about.
    // It stores an Entity VALUE, so a destroyed entity cannot be read through it: Get() answers "nothing selected".
    // ==================================================================
    class EditorSelection
    {
    public:
        // the selected entity, or an invalid Entity when nothing is selected or it has been destroyed
        Entity Get() const { return m_Entity.IsValid() ? m_Entity : Entity(); }
        bool   Has() const { return m_Entity.IsValid(); }
        bool   Is(Entity e) const { return e.IsValid() && e == m_Entity; }

        void Set(Entity e) { m_Entity = e; }
        void Clear() { m_Entity = Entity(); }

    private:
        Entity m_Entity;
    };
}
