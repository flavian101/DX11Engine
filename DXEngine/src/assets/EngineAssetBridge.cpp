#include "dxpch.h"
#include "EngineAssetBridge.h"
#include "models/ModelAsset.h"
#include "scene/Scene.h"
#include "RHI/GraphicsDevice.h"
#include "utils/AssetNames.h"
#include "utils/ImageLoader.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "utils/material/MaterialSlots.h"
#include <filesystem>

namespace DXEngine
{
    using AssetNames::ModelPart;

    EngineAssetBridge::EngineAssetBridge(std::shared_ptr<RHI::IGraphicsDevice> device, ModelLibrary& models, ModelImporter& importer,
                                         Assets::ImportLog& log, std::string root)
        : m_Device(std::move(device)), m_Models(models), m_Importer(importer), m_Log(log), m_Root(std::move(root))
    {
    }

    std::string EngineAssetBridge::StoredPathOf(const ImportedModel& model) const
    {
        return AssetNames::ToStoredPath(model.path, m_Root);
    }

    const ImportedModel* EngineAssetBridge::EnsureModel(const std::string& storedPath)
    {
        const std::string full = AssetNames::FromStoredPath(storedPath, m_Root);
        if (const ImportedModel* known = m_Models.Find(full)) return known;
        const ModelImporter::Result r = m_Importer.Import(m_Models, m_Log, full);     // the importer logs what happened
        return r.model;
    }

    // ---------------------------------------------------------------- naming
    std::string EngineAssetBridge::NameMesh(const Mesh& mesh)
    {
        if (!mesh.GetSourceName().empty()) return mesh.GetSourceName();
        for (const ImportedModel& model : m_Models.All())
            if (model.asset)
                for (size_t i = 0; i < model.asset->meshes.size(); ++i)
                    if (model.asset->meshes[i].mesh.get() == &mesh)
                        return AssetNames::ModelPartName(StoredPathOf(model), ModelPart::Mesh, i);
        return {};
    }

    std::string EngineAssetBridge::NameMaterial(const Material& material)
    {
        for (const ImportedModel& model : m_Models.All())
            if (model.asset)
                for (size_t i = 0; i < model.asset->materials.size(); ++i)
                    if (model.asset->materials[i].get() == &material)
                        return AssetNames::ModelPartName(StoredPathOf(model), ModelPart::Material, i);
        return {};
    }

    std::string EngineAssetBridge::NameModelOfSkeleton(const Skeleton& skeleton)
    {
        for (const ImportedModel& model : m_Models.All())
            if (model.asset && model.asset->skeleton.get() == &skeleton)
                return StoredPathOf(model);
        return {};
    }

    std::string EngineAssetBridge::NameTexture(const RHI::ITexture& texture, TextureSlot)
    {
        namespace fs = std::filesystem;
        const std::string& debugName = texture.GetDebugName();

        std::array<std::string, 6> faces;
        if (AssetNames::ParseCubeMapName(debugName, faces))
        {
            for (std::string& face : faces)
            {
                std::error_code ec;
                if (!fs::exists(fs::path(AssetNames::FromStoredPath(face, m_Root)), ec)) return {};    // a face that is gone
                face = AssetNames::ToStoredPath(face, m_Root);
            }
            return AssetNames::CubeMapName(faces);
        }

        // A texture loaded from a file carries the path it was loaded from as its name. Anything else ("SolidColor", a texture
        // embedded in a model) is not a file.
        std::error_code ec;
        if (debugName.empty() || !fs::is_regular_file(fs::path(AssetNames::FromStoredPath(debugName, m_Root)), ec)) return {};
        return AssetNames::ToStoredPath(debugName, m_Root);
    }

    // ---------------------------------------------------------------- resolving
    std::string EngineAssetBridge::TextureKey(const std::string& name, TextureSlot slot)
    {
        return name + "@" + std::to_string(static_cast<int>(MaterialSlots::Info(slot).colorSpace));
    }

    void EngineAssetBridge::Reuse(Scene& alive)
    {
        for (auto [entity, renderer] : alive.Registry().view<MeshRendererComponent>().each())
        {
            for (const std::shared_ptr<Material>& material : renderer.materials)
            {
                if (!material) continue;
                const auto& bound = material->GetTextures();
                for (const SlotInfo& info : MaterialSlots::All())
                {
                    const size_t i = static_cast<size_t>(info.slot);
                    if (i >= bound.size() || !bound[i]) continue;
                    const std::string name = NameTexture(*bound[i], info.slot);
                    if (!name.empty()) m_Textures.emplace(TextureKey(name, info.slot), bound[i]);     // the first holder wins
                }
            }
        }
    }

    std::shared_ptr<Mesh> EngineAssetBridge::ResolveMesh(const std::string& name)
    {
        AssetNames::Primitive p;
        if (AssetNames::ParsePrimitive(name, p))
        {
            switch (p.kind)
            {
            case AssetNames::Primitive::Kind::Cube:   return Mesh::CreateCube(m_Device, p.a);
            case AssetNames::Primitive::Kind::Quad:   return Mesh::CreateQuad(m_Device, p.a, p.b);
            case AssetNames::Primitive::Kind::Sphere: return Mesh::CreateSphere(m_Device, p.a, p.n);
            case AssetNames::Primitive::Kind::Plane:  return Mesh::CreatePlane(m_Device, p.a, p.b, p.n, p.m);
            default: break;
            }
            return nullptr;
        }

        std::string path; ModelPart part; size_t index;
        if (AssetNames::ParseModelPart(name, path, part, index) && part == ModelPart::Mesh)
        {
            const ImportedModel* model = EnsureModel(path);
            if (model && model->asset && index < model->asset->meshes.size()) return model->asset->meshes[index].mesh;
            m_Log.Warning("scene: mesh #" + std::to_string(index) + " of '" + path + "' does not exist" + (model ? "" : " (the model could not be imported)"));
            return nullptr;
        }
        m_Log.Warning("scene: the mesh name '" + name + "' is not understood");
        return nullptr;
    }

    std::shared_ptr<Material> EngineAssetBridge::ResolveMaterial(const std::string& name)
    {
        std::string path; ModelPart part; size_t index;
        if (AssetNames::ParseModelPart(name, path, part, index) && part == ModelPart::Material)
        {
            const ImportedModel* model = EnsureModel(path);
            if (model && model->asset && index < model->asset->materials.size()) return model->asset->materials[index];
            m_Log.Warning("scene: material #" + std::to_string(index) + " of '" + path + "' does not exist" + (model ? "" : " (the model could not be imported)"));
            return nullptr;
        }
        m_Log.Warning("scene: the material name '" + name + "' is not understood");
        return nullptr;
    }

    std::shared_ptr<ModelAsset> EngineAssetBridge::ResolveModel(const std::string& name)
    {
        const ImportedModel* model = EnsureModel(name);
        return model ? model->asset : nullptr;
    }

    std::shared_ptr<RHI::ITexture> EngineAssetBridge::ResolveTexture(const std::string& name, TextureSlot slot)
    {
        // the same file can be bound to a colour slot and a data slot: different formats, so different textures
        const std::string key = TextureKey(name, slot);
        if (const auto it = m_Textures.find(key); it != m_Textures.end()) return it->second;
        if (!m_Device) { m_Log.Warning("scene: no device, the texture '" + name + "' was not loaded"); return nullptr; }

        std::shared_ptr<RHI::ITexture> texture;
        std::array<std::string, 6> faces;
        if (AssetNames::ParseCubeMapName(name, faces))
        {
            for (std::string& face : faces) face = AssetNames::FromStoredPath(face, m_Root);
            texture = ImageLoader::LoadCubeMapForSlot(*m_Device, faces, slot);
        }
        else
            texture = ImageLoader::LoadForSlot(*m_Device, AssetNames::FromStoredPath(name, m_Root), slot);

        if (!texture) m_Log.Warning("scene: the texture '" + name + "' could not be loaded");
        else m_Textures.emplace(key, texture);
        return texture;
    }
}
