#pragma once
#include "assets/AssetCatalog.h"
#include "assets/ImportLog.h"
#include "assets/ModelImporter.h"
#include <functional>
#include <string>
#include <vector>

namespace DXEngine
{
    struct EditorContext;

    // ==================================================================
    // AssetsPanel: the model files in the assets folder, the models imported so far, and a log of what the importer did.
    //
    //   * double click a file (or right click > Add to scene): import it if needed, then put it into the scene in front of the
    //     camera and select it. A model that is already imported is not read again.
    //   * "Import file..." opens the system file dialog for a model anywhere on disk (also File > Import Model...).
    //   * "Imported" lists the library: its size, animations, warnings, an "Add to scene" and an "Unload" button.
    //
    // Importing blocks (the file is read and turned into GPU meshes on this thread). So a request is held for one frame:
    // that frame draws "Importing <file>..." and only the next one does the work, which is why the screen is not just frozen.
    //
    // Call Update every frame (it carries out a held request and draws that message, also while the window is hidden), and
    // OnImGui for the window itself.
    // ==================================================================
    class AssetsPanel
    {
    public:
        using FilePicker = std::function<std::string()>;

        AssetsPanel();

        void SetAssetsFolder(const std::string& folder);        // default "assets"
        const std::string& AssetsFolder() const { return m_Folder; }
        void SetFilePicker(FilePicker picker);                   // default: the system dialog (PlatformDialogs)
        ModelImporter& Importer() { return m_Importer; }
        const Assets::ImportLog& Log() const { return m_Log; }

        // ---- requests (carried out by Update, one frame later)
        void RequestImport(const std::string& path, bool addToScene, bool reload = false);
        void PickAndImport();                                    // asks the picker; a chosen file is imported and added to the scene
        bool IsBusy() const { return m_HasRequest; }
        void Rescan();                                           // read the assets folder again

        void Update(EditorContext& ctx);
        void OnImGui(EditorContext& ctx, bool* open = nullptr);

    private:
        struct Request { std::string path; bool addToScene = false; bool reload = false; int framesWaited = 0; };

        void Execute(EditorContext& ctx, const Request& request);
        void AddToScene(EditorContext& ctx, const std::string& path);
        void DrawFiles(EditorContext& ctx);
        void DrawImported(EditorContext& ctx);
        void DrawLog();
        std::string FullPath(const Assets::ModelFile& file) const;

        std::string                   m_Folder = "assets";
        std::vector<Assets::ModelFile> m_Files;
        bool                          m_Scanned = false;
        char                          m_Filter[64] = {};
        FilePicker                    m_Picker;

        ModelImporter                 m_Importer;
        Assets::ImportLog             m_Log;
        Request                       m_Request;
        bool                          m_HasRequest = false;
        std::string                   m_Status;                  // the last result, one line
        size_t                        m_LogSeen = 0;             // lines the log view has scrolled past
    };
}
