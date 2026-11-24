#pragma once
#include <string>

namespace DXEngine::PlatformDialogs
{
    // The system "Open file" dialog for a model (the formats the importer reads). Returns the chosen path (UTF-8), or an
    // empty string when the user cancelled. Blocks until the dialog closes. The current directory is left as it was
    // (the editor finds its assets relative to it).
    std::string OpenModelFile();

    // The system "Open file" dialog for an image (a material texture). Same contract as OpenModelFile: the chosen path
    // (UTF-8) or an empty string when cancelled; the current directory is left as it was.
    std::string OpenImageFile();

    // The system dialogs for a scene file (*.dxscene). OpenSceneFile: the chosen path (UTF-8) or "" when cancelled.
    // SaveSceneFile: the chosen path or "" when cancelled; 'suggested' is a file name or path that pre-fills the dialog; an
    // existing file asks for confirmation (the system does) and a name without an extension gets ".dxscene". Same contract as the
    // others: blocks until closed, the current directory is left as it was.
    std::string OpenSceneFile();
    std::string SaveSceneFile(const std::string& suggested);
}
