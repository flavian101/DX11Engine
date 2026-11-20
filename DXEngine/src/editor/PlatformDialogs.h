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
}
