#include "dxpch.h"
#include "ProjectRoot.h"

namespace DXEngine
{
    namespace fs = std::filesystem;

    bool ProjectRoot::IsRoot(const fs::path& folder)
    {
        std::error_code ec;
        return fs::is_directory(folder / "assets" / "shaders", ec);
    }

    std::optional<fs::path> ProjectRoot::Find(const std::vector<fs::path>& starts, int maxLevels)
    {
        for (const fs::path& start : starts)
        {
            std::error_code ec;
            fs::path folder = fs::absolute(start, ec);
            if (ec || !fs::is_directory(folder, ec)) continue;
            folder = folder.lexically_normal();
            if (!folder.has_filename()) folder = folder.parent_path();      // "a/b/" -> "a/b": parent_path() of a trailing slash is the same folder

            for (int level = 0; level <= maxLevels; ++level)
            {
                if (IsRoot(folder)) return folder;
                const fs::path parent = folder.parent_path();
                if (parent == folder || parent.empty()) break;               // the top of the drive
                folder = parent;
            }
        }
        return std::nullopt;
    }

    std::optional<fs::path> ProjectRoot::Enter(const std::vector<fs::path>& starts, int maxLevels)
    {
        const std::optional<fs::path> root = Find(starts, maxLevels);
        if (!root) return std::nullopt;
        std::error_code ec;
        fs::current_path(*root, ec);
        if (ec) return std::nullopt;
        return root;
    }
}
