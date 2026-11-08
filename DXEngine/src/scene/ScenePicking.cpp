#include "dxpch.h"
#include "ScenePicking.h"
#include "picking/Ray.h"
#include "utils/Mesh/Mesh.h"

namespace DXEngine
{
    namespace ScenePicking
    {
        PickResult Raycast(Scene& scene, const Ray& ray)
        {
            PickResult best;
            entt::registry& reg = scene.Registry();

            for (auto [entity, transform, renderer] : reg.view<TransformComponent, MeshRendererComponent>().each())
            {
                if (!renderer.visible || !renderer.mesh || !renderer.mesh->GetResource()) continue;
                if (reg.all_of<SkyboxComponent>(entity)) continue;

                const HitInfo hit = RayIntersection::IntersectMeshBoundingBox(ray, renderer.mesh->GetResource(), transform.World());
                if (hit.Hit && hit.Distance < best.distance)
                {
                    best.entity = scene.Wrap(entity);
                    best.distance = hit.Distance;
                }
            }
            return best;
        }

        PickResult Pick(Scene& scene, const Camera& camera, float screenX, float screenY, int screenWidth, int screenHeight)
        {
            return Raycast(scene, RayIntersection::ProjectRay(camera, screenX, screenY, screenWidth, screenHeight));
        }
    }
}
