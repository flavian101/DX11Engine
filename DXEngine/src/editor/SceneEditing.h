#pragma once
#include "scene/Scene.h"
#include <memory>
#include <string>

namespace DXEngine { namespace RHI { class IGraphicsDevice; } }

namespace DXEngine
{
    class ModelAsset;
    class Mesh;
    // ==================================================================
    // SceneEditing: the operations an editor performs on a Scene. Free functions over Scene / Entity, no UI and no GPU,
    // so each one is tested in editor_tests. (The panels call these; they never poke at components for structural changes.)
    // ==================================================================
    namespace SceneEditing
    {
        // "Cube" -> "Cube (Copy)"
        std::string CopyName(const std::string& name);

        // Whether Duplicate would work on this entity and everything below it. Only a sky refuses: there is one sky, and a
        // copy would be a second one. (An animated model can be duplicated: its playback state is plain data in the
        // Animator, and the copy gets a playback of its own.)
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

        // A mesh entity with a default grey lit material of its own (materials are shared assets: a new object must not
        // change the look of the ones already in the scene). Cube 1 x 1 x 1, sphere of diameter 1, plane 10 x 10.
        // 'device' uploads the mesh to the GPU; null keeps it on the CPU (tests do that).
        enum class Primitive { Cube, Sphere, Plane };
        const char* PrimitiveName(Primitive primitive);

        // Gives an EXISTING entity a mesh renderer showing a primitive (same default material rules as CreatePrimitive).
        // This is what "Add component > Mesh renderer" does. Returns false and changes nothing when the entity is
        // invalid, belongs to another scene, or already has a mesh renderer.
        bool AddMeshRenderer(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, Entity entity, Primitive primitive);

        // Gives an EXISTING entity a mesh renderer for 'mesh' (an imported model's mesh: it brings its own materials).
        // Returns false and changes nothing when the mesh is null, the entity is invalid or in another scene, or it already
        // has a mesh renderer.
        bool AddMeshRenderer(Scene& scene, Entity entity, const std::shared_ptr<Mesh>& mesh);

        // Puts an imported model into the scene as SceneFactory::Instantiate does, with the root at 'position'.
        // Returns an invalid Entity when the asset is null or empty.
        Entity InstantiateModel(Scene& scene, const std::shared_ptr<ModelAsset>& asset, const std::string& name,
                                const DirectX::XMFLOAT3& position);

        Entity CreatePrimitive(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, Primitive primitive, Entity parent = Entity());

        // A light stress test: 'count' point lights scattered over a square of 'extent' units around the origin, under one
        // parent entity ("Stress Test Lights"), so it shows as ONE row in the hierarchy and one Delete removes it all.
        // The same seed gives the same scene, so frame times of two runs compare. Returns the parent.
        Entity AddStressLights(Scene& scene, int count = 1000, float extent = 240.0f, unsigned seed = 1234);
    }
}
