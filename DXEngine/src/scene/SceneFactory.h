#pragma once
#include "Scene.h"
#include <array>
#include <memory>
#include <string>

namespace DXEngine
{
    class Mesh;
    class Material;
    class Model;
    namespace RHI { class IGraphicsDevice; }

    // ==================================================================
    // SceneFactory: the ways to put things into a Scene. These replace the game-object classes
    // (Ball, Ground, SkySphere, LightSphere): what used to be a subclass is now just a function that creates
    // an entity and gives it components. Nothing here is special-cased by the renderer.
    // ==================================================================
    namespace SceneFactory
    {
        // A visible object. 'material' (optional) is used for submesh 0 of THIS entity only; the mesh itself is not changed,
        // so several entities can share one mesh with different materials.
        Entity CreateMesh(Scene& scene, const std::string& name, std::shared_ptr<Mesh> mesh,
                          std::shared_ptr<Material> material = nullptr, Entity parent = Entity());

        Entity CreateSphere(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                            float radius, uint32_t segments, std::shared_ptr<Material> material = nullptr);

        Entity CreateCube(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                          float size, std::shared_ptr<Material> material = nullptr);

        Entity CreatePlane(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device, const std::string& name,
                           float width, float depth, std::shared_ptr<Material> material = nullptr);

        // A sky dome textured with six cube faces (see ImageLoader::LoadCubeMap for the face order).
        Entity CreateSky(Scene& scene, const std::shared_ptr<RHI::IGraphicsDevice>& device,
                         const std::array<std::string, 6>& faces, float radius = 50.0f);

        Entity CreatePointLight(Scene& scene, const std::string& name, const DirectX::XMFLOAT3& position,
                                const DirectX::XMFLOAT3& color, float intensity, float range);

        Entity CreateDirectionalLight(Scene& scene, const std::string& name, const DirectX::XMFLOAT3& direction,
                                      const DirectX::XMFLOAT3& color, float intensity);

        // Bridge from the model importer: a loaded Model becomes a root entity (with the model's transform) and
        // one child entity per mesh. Skinned models keep their animation: the root gets an AnimationUpdateComponent
        // and the meshes a SkinnedMeshComponent that read the Model's bones. Transitional, see Components.h.
        // Returns an invalid Entity if 'model' is null.
        Entity Instantiate(Scene& scene, const std::shared_ptr<Model>& model, const std::string& name);
    }
}
