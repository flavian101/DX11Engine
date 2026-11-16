#include "dxpch.h"
#include "SceneFactory.h"
#include "utils/ImageLoader.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"

namespace DXEngine
{
    namespace SceneFactory
    {
        Entity CreateMesh(Scene& scene, const std::string& name, std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material, Entity parent)
        {
            Entity entity = scene.CreateEntity(name, parent);
            auto& renderer = entity.Add<MeshRendererComponent>();
            renderer.mesh = std::move(mesh);
            if (material) renderer.materials = { std::move(material) };
            return entity;
        }

        Entity CreateSphere(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                            float radius, uint32_t segments, std::shared_ptr<Material> material)
        {
            return CreateMesh(scene, name, Mesh::CreateSphere(device, radius, segments), std::move(material));
        }

        Entity CreateCube(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                          float size, std::shared_ptr<Material> material)
        {
            return CreateMesh(scene, name, Mesh::CreateCube(device, size), std::move(material));
        }

        Entity CreatePlane(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                           float width, float depth, std::shared_ptr<Material> material)
        {
            return CreateMesh(scene, name, Mesh::CreatePlane(device, width, depth, 1, 1), std::move(material));
        }

        Entity CreateSky(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device,
                         const std::array<std::string, 6>& faces, float radius)
        {
            auto material = MaterialFactory::CreateSkyboxMaterial("SkyMaterial");
            if (device)
                material->SetTexture(TextureSlot::Environment, ImageLoader::LoadCubeMapForSlot(*device, faces, TextureSlot::Environment));

            Entity sky = CreateMesh(scene, "Sky", Mesh::CreateSphere(device, radius, 32), std::move(material));
            sky.Get<MeshRendererComponent>().castsShadows = false;
            sky.Get<MeshRendererComponent>().receivesShadows = false;
            sky.Add<SkyboxComponent>();
            return sky;
        }

        Entity CreatePointLight(Scene& scene, const std::string& name, const DirectX::XMFLOAT3& position,
                                const DirectX::XMFLOAT3& color, float intensity, float range)
        {
            Entity entity = scene.CreateEntity(name);
            entity.Get<TransformComponent>().position = position;
            auto& light = entity.Add<LightComponent>();
            light.type = LightType::Point;
            light.color = color;
            light.intensity = intensity;
            light.range = range;
            return entity;
        }

        Entity CreateDirectionalLight(Scene& scene, const std::string& name, const DirectX::XMFLOAT3& direction,
                                      const DirectX::XMFLOAT3& color, float intensity)
        {
            Entity entity = scene.CreateEntity(name);
            entity.Get<TransformComponent>().SetForward(direction);
            auto& light = entity.Add<LightComponent>();
            light.type = LightType::Directional;
            light.color = color;
            light.intensity = intensity;
            return entity;
        }
    }
}
