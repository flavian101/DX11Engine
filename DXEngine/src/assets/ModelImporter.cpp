#include "dxpch.h"
#include "ModelImporter.h"
#include "AssetCatalog.h"
#include "models/ModelAsset.h"
#include <chrono>
#include <filesystem>

namespace DXEngine
{
    ModelImporter::Result ModelImporter::Import(ModelLibrary& library, Assets::ImportLog& log, const std::string& path, bool reload)
    {
        Result result;
        const std::string name = std::filesystem::path(path).filename().string();

        if (!reload)
        {
            if (const ImportedModel* known = library.Find(path))
            {
                result.model = known;
                result.wasAlreadyImported = true;
                result.message = name + " is already imported";
                return result;
            }
        }

        if (!m_Load)
        {
            result.message = "Cannot import " + name + ": no importer is set up";
            log.Error(result.message);
            return result;
        }

        const auto start = std::chrono::steady_clock::now();
        std::string error;
        std::shared_ptr<ModelAsset> asset = m_Load(path, reload, error);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();

        if (!asset)
        {
            result.message = "Could not import " + name + (error.empty() ? "" : ": " + error);
            log.Error(result.message);
            return result;
        }

        const ImportedModel& entry = library.Add(path, asset, ms);
        result.model = &entry;

        std::string summary = std::string(reload ? "Reimported " : "Imported ") + name + ": " +
            std::to_string(asset->meshes.size()) + " meshes, " + std::to_string(asset->GetTotalTriangleCount()) + " triangles, " +
            std::to_string(asset->materials.size()) + " materials";
        if (asset->IsSkinned())
            summary += ", " + std::to_string(asset->clips.size()) + " animations";
        char time[32];
        snprintf(time, sizeof(time), " in %.0f ms", ms);
        result.message = summary + time;
        log.Info(result.message);
        for (const std::string& w : asset->warnings)
            log.Warning(name + ": " + w);
        if (!asset->warnings.empty())
            result.message += " (" + std::to_string(asset->warnings.size()) + " warnings)";
        return result;
    }
}
