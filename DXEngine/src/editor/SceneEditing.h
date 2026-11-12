#pragma once
#include "scene/Scene.h"
#include <string>

namespace DXEngine
{
    // ==================================================================
    // SceneEditing: the operations an editor performs on a Scene. Free functions over Scene / Entity, no UI and no GPU,
    // so each one is tested in editor_tests. (The panels call these; they never poke at components for structural changes.)
    // ==================================================================
    namespace SceneEditing
    {
        // "Cube" -> "Cube (Copy)"
        std::string CopyName(const std::string& name);

        // Whether Duplicate would work on this entity and everything below it. Skinned meshes and animation hooks are
        // bound to the imported model that owns their skeleton, a copy would share (and double-advance) it: those
        // subtrees cannot be duplicated yet. A skybox is a singleton: neither.
        bool CanDuplicate(Scene& scene, Entity entity);

        // Copies the entity and all its descendants next to the original (same parent). The root of the copy is named
        // "<name> (Copy)". Meshes and materials are SHARED with the original (they are assets), not copied.
        // Returns the new root, or an invalid Entity when CanDuplicate is false.
        Entity Duplicate(Scene& scene, Entity entity);

        // Makes 'newParent' the parent of 'child' (invalid = make it a root) WITHOUT moving it in the world: the local
        // transform is recomputed. (Scene::SetParent alone keeps the local transform, so the entity jumps.)
        // Returns false and changes nothing for a cycle (below itself) or an entity of another scene.
        bool Reparent(Scene& scene, Entity child, Entity newParent);

        // Puts the entity at 'world' (a matrix in world space): writes the local position / rotation / scale that give
        // that, given the parent's current world matrix. This is what a gizmo hands back. Returns false if it could not.
        bool ApplyWorldMatrix(Scene& scene, Entity entity, const DirectX::XMFLOAT4X4& world);

        // New entities. All are named, have a transform, and go under 'parent' when it is valid.
        Entity CreateEmpty(Scene& scene, const std::string& name, Entity parent = Entity());
        Entity CreateLight(Scene& scene, LightType type, Entity parent = Entity());
    }
}
