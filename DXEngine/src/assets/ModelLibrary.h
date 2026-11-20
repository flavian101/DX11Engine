#pragma once
// ============================================================================
// ModelLibrary: the models imported in this session, in the order they were imported. It keeps each ModelAsset alive (the
// instances in the scene share its meshes and materials either way, but the library is what lets a model be instantiated
// again, or its meshes be added to an entity, without importing the file once more).
//
// Keyed by the normalised path (AssetCatalog::NormalizePath): two spellings of one file are one entry.
// ============================================================================
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace DXEngine
{
    class ModelAsset;

    struct ImportedModel
    {
        std::string                 key;        // the normalised path
        std::string                 path;       // as it was given
        std::string                 name;       // the file name without folders and extension
        std::shared_ptr<ModelAsset> asset;
        double                      importMs = 0.0;
        size_t                      instances = 0;   // how many times it was put into the scene from here
    };

    class ModelLibrary
    {
    public:
        // Adds the model, or REPLACES the entry of the same file (a re-import) keeping its place and its instance count.
        // Returns the entry; the reference stays valid until the next Add or Remove.
        ImportedModel& Add(const std::string& path, std::shared_ptr<ModelAsset> asset, double importMs);

        const ImportedModel* Find(const std::string& path) const;
        bool Contains(const std::string& path) const { return Find(path) != nullptr; }
        bool Remove(const std::string& path);                 // the instances in the scene keep working: they share the data
        void NoteInstance(const std::string& path);

        const std::vector<ImportedModel>& All() const { return m_Models; }
        size_t Count() const { return m_Models.size(); }
        void Clear() { m_Models.clear(); }

    private:
        std::vector<ImportedModel> m_Models;
    };
}
