#include "dxpch.h"
#include "SceneEditing.h"
#include "TransformEditing.h"
#include "scene/TransformSystem.h"

namespace DXEngine
{
    namespace SceneEditing
    {
        namespace
        {
            bool SubtreeAllows(Scene& scene, Entity e)
            {
                if (e.Has<SkinnedMeshComponent>() || e.Has<AnimationUpdateComponent>() || e.Has<SkyboxComponent>())
                    return false;
                for (Entity child : scene.GetChildren(e))
                    if (!SubtreeAllows(scene, child))
                        return false;
                return true;
            }

            // Copies 'source' under 'parent' (invalid = a root) and recurses into its children.
            Entity CloneSubtree(Scene& scene, Entity source, Entity parent, const std::string& name)
            {
                // Copy the values FIRST: creating an entity can grow the component pools, which would leave references
                // into them dangling.
                const TransformComponent transform = source.Get<TransformComponent>();
                const bool hasRenderer = source.Has<MeshRendererComponent>();
                const MeshRendererComponent renderer = hasRenderer ? source.Get<MeshRendererComponent>() : MeshRendererComponent{};
                const bool hasLight = source.Has<LightComponent>();
                const LightComponent light = hasLight ? source.Get<LightComponent>() : LightComponent{};
                const std::vector<Entity> children = scene.GetChildren(source);

                Entity copy = scene.CreateEntity(name, parent);
                copy.Get<TransformComponent>() = transform;       // local TRS and the current world matrix
                if (hasRenderer) copy.Add<MeshRendererComponent>(renderer);
                if (hasLight)    copy.Add<LightComponent>(light);

                for (Entity child : children)
                    CloneSubtree(scene, child, copy, child.Get<NameComponent>().name);
                return copy;
            }
        }

        std::string CopyName(const std::string& name) { return name + " (Copy)"; }

        bool CanDuplicate(Scene& scene, Entity entity)
        {
            return entity.IsValid() && entity.GetScene() == &scene && SubtreeAllows(scene, entity);
        }

        Entity Duplicate(Scene& scene, Entity entity)
        {
            if (!CanDuplicate(scene, entity)) return Entity();
            return CloneSubtree(scene, entity, scene.GetParent(entity), CopyName(entity.Get<NameComponent>().name));
        }

        bool Reparent(Scene& scene, Entity child, Entity newParent)
        {
            if (!child.IsValid() || child.GetScene() != &scene) return false;
            if (newParent.IsValid() && newParent.GetScene() != &scene) return false;

            // world matrices must be current: they are what is preserved
            TransformSystem::Update(scene);

            using namespace DirectX;
            const XMMATRIX world = child.Get<TransformComponent>().World();
            const XMMATRIX parentWorld = newParent.IsValid() ? newParent.Get<TransformComponent>().World() : XMMatrixIdentity();

            if (!scene.SetParent(child, newParent))      // refuses a cycle
                return false;

            // If the parent cannot be inverted (zero scale) the local transform stays as it was: still reparented, but it moves.
            TransformComponent local = child.Get<TransformComponent>();
            if (TransformEditing::ComputeLocalFromWorld(world, parentWorld, local))
                child.Get<TransformComponent>() = local;
            return true;
        }

        bool ApplyWorldMatrix(Scene& scene, Entity entity, const DirectX::XMFLOAT4X4& world)
        {
            if (!entity.IsValid() || entity.GetScene() != &scene) return false;

            using namespace DirectX;
            Entity parent = scene.GetParent(entity);
            const XMMATRIX parentWorld = parent.IsValid() ? parent.Get<TransformComponent>().World() : XMMatrixIdentity();

            TransformComponent& t = entity.Get<TransformComponent>();
            if (!TransformEditing::ComputeLocalFromWorld(XMLoadFloat4x4(&world), parentWorld, t))
                return false;
            t.world = world;       // right until the next TransformSystem::Update, so the gizmo does not lag a frame
            return true;
        }

        Entity CreateEmpty(Scene& scene, const std::string& name, Entity parent)
        {
            return parent.IsValid() ? scene.CreateEntity(name, parent) : scene.CreateEntity(name);
        }

        Entity CreateLight(Scene& scene, LightType type, Entity parent)
        {
            const char* name = type == LightType::Directional ? "Directional Light"
                             : type == LightType::Spot        ? "Spot Light"
                                                              : "Point Light";
            Entity e = CreateEmpty(scene, name, parent);

            LightComponent light;
            light.type = type;
            e.Add<LightComponent>(light);

            TransformComponent& t = e.Get<TransformComponent>();
            if (type == LightType::Directional)
                t.SetEulerDegrees(50.0f, -30.0f, 0.0f);            // shines down and a little sideways, like a sun
            else
                t.position = { 0.0f, 3.0f, 0.0f };
            if (type == LightType::Spot)
                t.SetEulerDegrees(90.0f, 0.0f, 0.0f);              // +Z is the light direction: pitch 90 points it straight down
            return e;
        }
    }
}
