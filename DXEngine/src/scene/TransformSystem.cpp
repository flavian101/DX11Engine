#include "dxpch.h"
#include "TransformSystem.h"
#include "Scene.h"
#include <algorithm>

namespace DXEngine
{
    namespace TransformSystem
    {
        void Update(Scene& scene)
        {
            entt::registry& reg = scene.Registry();

            // An entity must be processed after its parent. Instead of recursing, order by depth in the
            // hierarchy (roots = 0). The depth walk is capped so a corrupted hierarchy cannot loop forever
            // (Scene::SetParent already refuses to create cycles).
            struct Item { entt::entity entity; uint32_t depth; };
            std::vector<Item> items;
            items.reserve(reg.view<TransformComponent>().size());

            for (auto entity : reg.view<TransformComponent>())
            {
                uint32_t depth = 0;
                for (const ParentComponent* up = reg.try_get<ParentComponent>(entity); up && depth < 1024; )
                {
                    ++depth;
                    up = reg.valid(up->parent) ? reg.try_get<ParentComponent>(up->parent) : nullptr;
                }
                items.push_back({ entity, depth });
            }
            std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) { return a.depth < b.depth; });

            for (const Item& item : items)
            {
                TransformComponent& t = reg.get<TransformComponent>(item.entity);
                DirectX::XMMATRIX world = t.Local();

                // child world = local * parent world (row-vector convention)
                if (const ParentComponent* up = reg.try_get<ParentComponent>(item.entity))
                    if (reg.valid(up->parent))
                        if (const TransformComponent* parentT = reg.try_get<TransformComponent>(up->parent))
                            world = world * parentT->World();

                DirectX::XMStoreFloat4x4(&t.world, world);
            }
        }
    }
}
