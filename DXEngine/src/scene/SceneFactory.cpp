#include "dxpch.h"
#include "SceneFactory.h"
#include "models/Model.h"
#include "models/ModelFeatures.h"
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

        Entity Instantiate(Scene& scene, const std::shared_ptr<Model>& model, const std::string& name)
        {
            if (!model) return Entity();

            Entity root = scene.CreateEntity(name);
            {
                auto& t = root.Get<TransformComponent>();
                DirectX::XMStoreFloat3(&t.position, model->GetTranslation());
                DirectX::XMStoreFloat3(&t.scale, model->GetScale());
                DirectX::XMStoreFloat4(&t.rotation, model->GetRotation());
            }

            // Skinned model: the Model still owns the skeleton and the animation controller. The scene keeps it alive
            // (captured below) and asks for the bones every frame; a missing skeleton simply means "static".
            const bool skinned = model->IsSkinned();
            if (skinned)
            {
                root.Add<AnimationUpdateComponent>().update = [model](float dt) { model->Update(dt); };
            }

            for (size_t i = 0; i < model->GetMeshCount(); ++i)
            {
                std::shared_ptr<Mesh> mesh = model->GetMesh(i);
                if (!mesh) continue;

                // no material list: the mesh's own materials (assigned by the importer) are used
                Entity child = CreateMesh(scene, name + "_mesh" + std::to_string(i), std::move(mesh), nullptr, root);
                if (skinned)
                {
                    child.Add<SkinnedMeshComponent>().boneMatrices = [model]() -> const std::vector<DirectX::XMFLOAT4X4>* {
                        const SkinningData* data = model->GetSkinningData();
                        return (data && !data->boneMatrices.empty()) ? &data->boneMatrices : nullptr;
                    };
                }
            }
            return root;
        }
    }
}
