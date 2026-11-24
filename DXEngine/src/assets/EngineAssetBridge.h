#pragma once
#include "assets/ImportLog.h"
#include "assets/ModelImporter.h"
#include "assets/ModelLibrary.h"
#include "scene/SceneSerializer.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace DXEngine
{
    namespace RHI { class IGraphicsDevice; }

    // ==================================================================
    // EngineAssetBridge: the IAssetBridge the editor (and a game that loads scenes) uses. It names and finds assets with the
    // engine's own services:
    //
    //   generated meshes   "primitive/cube?size=2": Mesh::CreateCube(...) and friends (the mesh remembers the name it was made with)
    //   imported models    "model/<path>#mesh/3", "#material/0": looked up in the ModelLibrary, imported through the
    //                      ModelImporter when the library does not have the file yet (what the import did goes to the log)
    //   textures           the file path (relative to the root when under it), or "cube:<six paths>"; loaded through ImageLoader
    //                      with the colour space of the slot they are bound to; one texture per (name, colour space)
    //
    // 'root' is the folder names are relative to (the project folder; the editor uses the working directory). Paths are written
    // relative to it when the file is inside it, so a scene moves between machines with its assets.
    //
    // 'device' may be null: naming works, and so does creating generated meshes (they upload when first drawn); textures cannot be
    // loaded then (ResolveTexture returns null and says so in the log).
    //
    // What cannot be saved: a mesh that is neither generated nor part of a library model, a texture that is not a file or a cube
    // map made of files (a texture embedded in a model belongs to the model's materials, which are saved by name). Naming returns ""
    // for them and the scene serializer says so.
    //
    // KNOWN LIMIT: edits made to a MODEL's material (its roughness, say) are not part of the scene: the scene says "the material
    // that model has", and the model's own values come back on load. A material of your own (created or duplicated in the editor)
    // is saved by value.
    // ==================================================================
    class EngineAssetBridge : public IAssetBridge
    {
    public:
        EngineAssetBridge(std::shared_ptr<RHI::IGraphicsDevice> device, ModelLibrary& models, ModelImporter& importer,
                          Assets::ImportLog& log, std::string root);

        std::string NameMesh(const Mesh& mesh) override;
        std::string NameMaterial(const Material& material) override;
        std::string NameTexture(const RHI::ITexture& texture, TextureSlot slot) override;
        std::string NameModelOfSkeleton(const Skeleton& skeleton) override;

        std::shared_ptr<Mesh> ResolveMesh(const std::string& name) override;
        std::shared_ptr<Material> ResolveMaterial(const std::string& name) override;
        std::shared_ptr<RHI::ITexture> ResolveTexture(const std::string& name, TextureSlot slot) override;
        std::shared_ptr<ModelAsset> ResolveModel(const std::string& name) override;

    private:
        // The library entry for a stored model path, importing the file first when the library has not seen it.
        const ImportedModel* EnsureModel(const std::string& storedPath);
        std::string StoredPathOf(const ImportedModel& model) const;

        std::shared_ptr<RHI::IGraphicsDevice> m_Device;
        ModelLibrary&      m_Models;
        ModelImporter&     m_Importer;
        Assets::ImportLog& m_Log;
        std::string        m_Root;
        std::unordered_map<std::string, std::shared_ptr<RHI::ITexture>> m_Textures;   // by name + colour space
    };
}
