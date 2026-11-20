#pragma once
// ============================================================================
// ModelImporter: imports a model file into a ModelLibrary and says what happened. The file reading is a function it is
// given (the editor gives it the engine's ModelLoader: assets/ModelLoaderAdapter.h), so the flow around it (already
// imported? reload? a file that fails? what is logged) is tested without a GPU or assimp.
// ============================================================================
#include "ImportLog.h"
#include "ModelLibrary.h"
#include <functional>
#include <memory>
#include <string>

namespace DXEngine
{
    class ModelImporter
    {
    public:
        // Reads 'path' and returns the asset, or null with 'error' filled in. 'reload': skip any cache the loader keeps.
        using LoadFunction = std::function<std::shared_ptr<ModelAsset>(const std::string& path, bool reload, std::string& error)>;

        // The file reader. Without one every import fails with a message saying so.
        void SetLoadFunction(LoadFunction load) { m_Load = std::move(load); }
        bool HasLoadFunction() const { return static_cast<bool>(m_Load); }

        struct Result
        {
            const ImportedModel* model = nullptr;      // null when the import failed
            bool   wasAlreadyImported = false;         // no file was read: the library had it
            std::string message;                       // one line for the status bar
        };

        // Brings 'path' into 'library'. A model that is already there is not read again unless 'reload'. Everything that
        // happened (the summary, each import warning, a failure) is added to 'log'.
        Result Import(ModelLibrary& library, Assets::ImportLog& log, const std::string& path, bool reload = false);

    private:
        LoadFunction m_Load;
    };
}
