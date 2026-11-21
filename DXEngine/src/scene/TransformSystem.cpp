#include "dxpch.h"
#include "TransformSystem.h"
#include "Scene.h"

namespace DXEngine
{
    namespace TransformSystem
    {
        namespace
        {
            // true if an ancestor of 'entity' (that has a transform; the chain of inherited transforms ends at the first
            // one that has none) is going to be recomputed, which then recomputes 'entity' too.
            bool AncestorIsDirty(entt::registry& reg, entt::entity entity)
            {
                const RelationshipComponent* rel = reg.try_get<RelationshipComponent>(entity);
                // (a cycle cannot exist, Scene::SetParent refuses it; the bound is a belt)
                for (int depth = 0; rel && rel->parent != entt::null && reg.valid(rel->parent) && depth < 4096; ++depth)
                {
                    const TransformComponent* parent = reg.try_get<TransformComponent>(rel->parent);
                    if (!parent) return false;
                    if (parent->tracking.dirty) return true;
                    rel = reg.try_get<RelationshipComponent>(rel->parent);
                }
                return false;
            }

            // Recomputes 'root' and everything below it, parents before children, by following the child links.
            // The parent of 'root' must be current (nothing above root changed). Returns the number of matrices written.
            std::size_t RecomputeSubtree(entt::registry& reg, entt::entity root, std::vector<entt::entity>& stack)
            {
                const std::size_t bound = reg.storage<TransformComponent>().size();   // a subtree cannot have more entities than exist
                std::size_t written = 0;

                stack.clear();
                stack.push_back(root);
                while (!stack.empty() && written <= bound)
                {
                    const entt::entity e = stack.back();
                    stack.pop_back();

                    TransformComponent& t = reg.get<TransformComponent>(e);
                    DirectX::XMMATRIX world = t.Local();

                    const RelationshipComponent* rel = reg.try_get<RelationshipComponent>(e);
                    if (rel && rel->parent != entt::null && reg.valid(rel->parent))
                        if (const TransformComponent* parent = reg.try_get<TransformComponent>(rel->parent))
                            world = world * parent->World();                      // row vectors: child world = local * parent world

                    DirectX::XMStoreFloat4x4(&t.world, world);
                    ++written;

                    if (rel)
                        for (entt::entity c = rel->firstChild; c != entt::null && reg.valid(c); )
                        {
                            if (reg.all_of<TransformComponent>(c)) stack.push_back(c);
                            const RelationshipComponent* childRel = reg.try_get<RelationshipComponent>(c);
                            c = childRel ? childRel->nextSibling : entt::null;
                        }
                }
                return written;
            }
        }

        bool IsCurrent(Scene& scene)
        {
            for (auto [e, t] : scene.Registry().view<TransformComponent>().each())
                if (t.NeedsUpdate()) return false;
            return true;
        }

        std::size_t Update(Scene& scene)
        {
            entt::registry& reg = scene.Registry();

            // 1. What changed? Flag it, so that step 2 can tell "my ancestor is being recomputed anyway".
            std::vector<entt::entity> changed;
            for (auto [e, t] : reg.view<TransformComponent>().each())
                if (t.NeedsUpdate())
                {
                    t.tracking.dirty = true;
                    changed.push_back(e);
                }
            if (changed.empty()) return 0;

            // 2. Recompute every topmost changed entity with its subtree. A changed entity below another changed one is
            //    skipped: its ancestor's subtree covers it, once.
            std::size_t recomputed = 0;
            std::vector<entt::entity> stack;
            for (entt::entity e : changed)
                if (!AncestorIsDirty(reg, e))
                    recomputed += RecomputeSubtree(reg, e, stack);

            // 3. All world matrices are current: remember what they were computed from.
            for (entt::entity e : changed)
            {
                TransformComponent& t = reg.get<TransformComponent>(e);
                t.tracking.position = t.position;
                t.tracking.rotation = t.rotation;
                t.tracking.scale    = t.scale;
                t.tracking.dirty    = false;
            }
            return recomputed;
        }
    }
}
