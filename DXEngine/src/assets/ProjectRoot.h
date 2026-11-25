#pragma once
#include <filesystem>
#include <optional>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // ProjectRoot: the folder the application's files are found from. Shaders (assets/shaders/), textures and models
    // (assets/...), scene files (stored relative to it), editor_prefs.ini and the editor layout all sit under it.
    //
    // The root used to be "whatever the working directory is", so an application only started from the right folder (B8).
    // Now it is FOUND: the nearest folder that has assets/shaders, looking from the executable's folder upwards, then from the
    // working directory upwards. The application enters it once at start-up (Main.h), so it starts the same way from Visual
    // Studio, a shortcut or a terminal, and relative paths mean the same thing wherever it was launched from.
    //
    // The marker is assets/shaders because no application runs without them. Nothing here touches anything but directories.
    // ==================================================================
    namespace ProjectRoot
    {
        // A root is a folder that has a folder assets/shaders.
        bool IsRoot(const std::filesystem::path& folder);

        // For each start in order: the start itself, then its parents (at most 'maxLevels' of them), the first root wins.
        // Starts that do not exist are skipped. Nothing found: nullopt. The result is absolute.
        std::optional<std::filesystem::path> Find(const std::vector<std::filesystem::path>& starts, int maxLevels = 8);

        // Find(), then make it the working directory. The root, or nullopt (nothing found, or the folder could not be entered:
        // the working directory is then as it was).
        std::optional<std::filesystem::path> Enter(const std::vector<std::filesystem::path>& starts, int maxLevels = 8);
    }
}
