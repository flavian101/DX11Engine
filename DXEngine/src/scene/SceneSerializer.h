#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class Scene;
    class Mesh;
    class Material;
    class ModelAsset;
    class Skeleton;
    namespace RHI { class ITexture; }
    enum class TextureSlot : uint32_t;

    // ==================================================================
    // IAssetBridge: how a scene file talks about assets.
    //
    // A scene file holds the ENTITIES and what is attached to them. It does not hold meshes, textures or imported models:
    // it holds NAMES for them ("primitive/cube?size=2", "models/horse/Horse.fbx", "textures/grass.jpg"). The bridge turns
    // the scene's objects into names when saving and the names back into objects when loading. The serializer treats every
    // name as an opaque string; the bridge decides what they look like and where the data comes from, so the same
    // serializer works with the real asset loaders (a device, files) and with a test that has neither.
    //
    // Naming (saving). An empty string means "this object has no name that a file could use": for a mesh or a texture the
    // serializer leaves the thing out and says so in the warnings; for a MATERIAL it is not a failure, the material's values
    // are then written into the scene file itself.
    //
    // Resolving (loading). Null means "could not be loaded": the serializer drops that attachment, keeps the entity and
    // warns. The serializer asks once per distinct name within one load and shares the result, so entities that shared a mesh
    // when saved share it again; a bridge should cache too (the same texture named by two materials).
    // ==================================================================
    class IAssetBridge
    {
    public:
        virtual ~IAssetBridge() = default;

        // scene -> file
        virtual std::string NameMesh(const Mesh& mesh) = 0;
        virtual std::string NameMaterial(const Material& material) = 0;     // "" = write the values inline
        virtual std::string NameTexture(const RHI::ITexture& texture, TextureSlot slot) = 0;
        virtual std::string NameModelOfSkeleton(const Skeleton& skeleton) = 0;   // the model an Animator's skeleton came from

        // file -> scene
        virtual std::shared_ptr<Mesh> ResolveMesh(const std::string& name) = 0;
        virtual std::shared_ptr<Material> ResolveMaterial(const std::string& name) = 0;   // a material that belongs to a named model
        virtual std::shared_ptr<RHI::ITexture> ResolveTexture(const std::string& name, TextureSlot slot) = 0;
        virtual std::shared_ptr<ModelAsset> ResolveModel(const std::string& name) = 0;
    };

    // ==================================================================
    // SceneSerializer: a scene <-> text (JSON, see docs/ROADMAP.md for the format).
    //
    // What is saved: every entity, with its id, name, parent and the order of its siblings, its local transform, and the
    // components Light, MeshRenderer (mesh, one material per submesh, the visible / shadow flags), SkinnedMesh, Skybox and
    // Animator (its model and its settings). What is NOT saved: world matrices and bone matrices (derived: run
    // TransformSystem / AnimationSystem after loading), and the animator's controller (a cache).
    //
    // Loading is all or nothing for the FILE (a malformed one, or one made by a newer version, changes nothing) and best
    // effort for its CONTENT: an attachment that cannot be resolved (a missing texture file, an unknown component value) is
    // dropped with a warning and the rest of the scene loads. Unknown keys are ignored, so older builds read newer files
    // as far as they can.
    // ==================================================================
    namespace SceneSerializer
    {
        constexpr int kVersion = 1;

        struct SaveResult
        {
            std::string text;
            std::vector<std::string> warnings;     // things that could not be written (a mesh without a name, ...)
        };

        // The caller's scene is only read. World matrices are not needed.
        SaveResult Save(Scene& scene, IAssetBridge& bridge);

        struct LoadResult
        {
            bool ok = false;
            std::string error;                     // why nothing was loaded ("line:column: ..." for malformed text)
            std::vector<std::string> warnings;     // what was dropped or repaired while loading
            size_t entities = 0;
        };

        // Replaces the contents of 'into'. On failure 'into' is untouched. Entity handles and ids of the old contents are
        // invalid afterwards (clear the selection). The loaded world matrices are NOT current: run TransformSystem::Update.
        LoadResult Load(const std::string& text, Scene& into, IAssetBridge& bridge);
    }
}
