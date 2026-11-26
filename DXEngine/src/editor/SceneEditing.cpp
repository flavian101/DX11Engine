#include "dxpch.h"
#include "SceneEditing.h"
#include "scene/SceneFactory.h"
#include "TransformEditing.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <random>

namespace DXEngine
{
    namespace SceneEditing
    {
        namespace
        {
            bool SubtreeAllows(Scene& scene, Entity e)
            {
                if (e.Has<SkyboxComponent>())
                    return false;                                 // there is one sky: a copy would be a second one
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
                const bool hasAnimator = source.Has<AnimatorComponent>();
                const AnimatorComponent animator = hasAnimator ? source.Get<AnimatorComponent>() : AnimatorComponent{};   // the copy gets its own playback (see AnimatorRuntime)
                const bool isSkinned = source.Has<SkinnedMeshComponent>();
                const std::vector<Entity> children = scene.GetChildren(source);

                Entity copy = scene.CreateEntity(name, parent);
                copy.Get<TransformComponent>() = transform;       // local TRS and the current world matrix
                if (hasRenderer) copy.Add<MeshRendererComponent>(renderer);
                if (hasLight)    copy.Add<LightComponent>(light);
                if (hasAnimator) copy.Add<AnimatorComponent>(animator);
                if (isSkinned)   copy.Add<SkinnedMeshComponent>();

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

        std::vector<Entity> AllEntities(Scene& scene)
        {
            std::vector<Entity> all;
            all.reserve(scene.EntityCount());
            for (auto [id, name] : scene.Registry().view<NameComponent>().each())
            {
                Entity e = scene.Wrap(id);
                if (!e.Has<SkyboxComponent>()) all.push_back(e);
            }
            return all;
        }

        std::vector<Entity> Roots(Scene& scene, const std::vector<Entity>& entities)
        {
            std::vector<Entity> valid;
            for (const Entity& e : entities)
                if (e.IsValid() && e.GetScene() == &scene && std::find(valid.begin(), valid.end(), e) == valid.end())
                    valid.push_back(e);

            std::vector<Entity> roots;
            for (const Entity& e : valid)
            {
                bool underAnother = false;
                for (Entity p = scene.GetParent(e); p.IsValid() && !underAnother; p = scene.GetParent(p))
                    underAnother = std::find(valid.begin(), valid.end(), p) != valid.end();
                if (!underAnother) roots.push_back(e);
            }
            return roots;
        }

        size_t DestroyEntities(Scene& scene, const std::vector<Entity>& entities)
        {
            const std::vector<Entity> roots = Roots(scene, entities);
            for (Entity e : roots) scene.DestroyEntity(e);           // its descendants go with it
            return roots.size();
        }

        bool CanDuplicateAny(Scene& scene, const std::vector<Entity>& entities)
        {
            for (const Entity& e : Roots(scene, entities))
                if (CanDuplicate(scene, e)) return true;
            return false;
        }

        std::vector<Entity> DuplicateEntities(Scene& scene, const std::vector<Entity>& entities)
        {
            std::vector<Entity> copies;
            for (Entity e : Roots(scene, entities))
            {
                Entity copy = Duplicate(scene, e);
                if (copy.IsValid()) copies.push_back(copy);
            }
            return copies;
        }

        size_t ReparentEntities(Scene& scene, const std::vector<Entity>& entities, Entity newParent)
        {
            size_t moved = 0;
            for (Entity e : Roots(scene, entities))
                if (e != newParent && Reparent(scene, e, newParent)) ++moved;      // (a cycle is refused by Reparent)
            return moved;
        }

        bool ApplyWorldDelta(Scene& scene, const std::vector<Entity>& entities, const DirectX::XMFLOAT4X4& before, const DirectX::XMFLOAT4X4& after)
        {
            using namespace DirectX;
            const XMMATRIX oldWorld = XMLoadFloat4x4(&before);
            XMVECTOR det;
            const XMMATRIX inverse = XMMatrixInverse(&det, oldWorld);
            if (std::fabs(XMVectorGetX(det)) < 1e-12f) return false;

            // row vectors: new = old * delta, so a point moves by delta = old^-1 * new, whatever entity it belongs to
            const XMMATRIX delta = XMMatrixMultiply(inverse, XMLoadFloat4x4(&after));
            for (Entity e : Roots(scene, entities))
            {
                if (!e.Has<TransformComponent>()) continue;
                XMFLOAT4X4 world;
                XMStoreFloat4x4(&world, XMMatrixMultiply(e.Get<TransformComponent>().World(), delta));
                ApplyWorldMatrix(scene, e, world);
            }
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

        const char* PrimitiveName(Primitive primitive)
        {
            return primitive == Primitive::Cube ? "Cube" : primitive == Primitive::Sphere ? "Sphere" : "Plane";
        }

        bool AddMeshRenderer(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, Entity entity, Primitive primitive)
        {
            if (!entity.IsValid() || entity.GetScene() != &scene || entity.Has<MeshRendererComponent>())
                return false;

            const char* name = PrimitiveName(primitive);
            std::shared_ptr<Mesh> mesh = primitive == Primitive::Cube   ? Mesh::CreateCube(device, 1.0f)
                                       : primitive == Primitive::Sphere ? Mesh::CreateSphere(device, 0.5f, 32)
                                                                        : Mesh::CreatePlane(device, 10.0f, 10.0f);
            auto material = MaterialFactory::CreateLitMaterial(std::string(name) + " Material");
            material->SetDiffuseColor({ 0.80f, 0.80f, 0.82f, 1.0f });
            material->SetRoughness(0.6f);

            auto& renderer = entity.Add<MeshRendererComponent>();
            renderer.mesh = std::move(mesh);
            renderer.materials = { std::move(material) };
            return true;
        }

        bool AddMeshRenderer(Scene& scene, Entity entity, const std::shared_ptr<Mesh>& mesh)
        {
            if (!mesh || !entity.IsValid() || entity.GetScene() != &scene || entity.Has<MeshRendererComponent>())
                return false;
            entity.Add<MeshRendererComponent>().mesh = mesh;       // no material list: the mesh's own materials are used
            return true;
        }

        Entity InstantiateModel(Scene& scene, const std::shared_ptr<ModelAsset>& asset, const std::string& name,
                                const DirectX::XMFLOAT3& position)
        {
            Entity root = SceneFactory::Instantiate(scene, asset, name);
            if (root.IsValid())
                root.Get<TransformComponent>().position = position;
            return root;
        }

        Entity CreatePrimitive(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, Primitive primitive, Entity parent)
        {
            Entity e = CreateEmpty(scene, PrimitiveName(primitive), parent);
            AddMeshRenderer(scene, device, e, primitive);
            return e;
        }

        Entity AddStressLights(Scene& scene, int count, float extent, unsigned seed)
        {
            Entity group = scene.CreateEntity("Stress Test Lights");
            std::mt19937 rng(seed);
            std::uniform_real_distribution<float> spread(-0.5f * extent, 0.5f * extent), height(1.0f, 9.0f),
                                                  range(8.0f, 16.0f), channel(0.2f, 1.0f);
            for (int i = 0; i < count; ++i)
            {
                Entity e = scene.CreateEntity("Stress Light " + std::to_string(i + 1), group);
                e.Get<TransformComponent>().position = { spread(rng), height(rng), spread(rng) };
                LightComponent light;
                light.type = LightType::Point;
                light.color = { channel(rng), channel(rng), channel(rng) };
                light.intensity = 60.0f;
                light.range = range(rng);
                e.Add<LightComponent>(light);
            }
            return group;
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
