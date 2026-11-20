#include "AssetCatalog.h"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace DXEngine::Assets
{
    namespace
    {
        std::string Lower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        const std::vector<std::string>& Extensions()
        {
            static const std::vector<std::string> kExtensions = {
                ".fbx", ".obj", ".dae", ".gltf", ".glb", ".3ds", ".ase", ".ply",
                ".x", ".md3", ".md2", ".md5mesh", ".iqm", ".bvh", ".csm" };
            return kExtensions;
        }

        std::vector<std::string> Components(const std::string& folder)
        {
            std::vector<std::string> parts;
            size_t start = 0;
            while (start <= folder.size())
            {
                const size_t end = folder.find('/', start);
                parts.push_back(Lower(folder.substr(start, end == std::string::npos ? std::string::npos : end - start)));
                if (end == std::string::npos) break;
                start = end + 1;
            }
            return parts;
        }

        std::string Slashes(std::string s)
        {
            std::replace(s.begin(), s.end(), '\\', '/');
            return s;
        }
    }

    bool IsModelExtension(const std::string& extension)
    {
        std::string e = Lower(extension);
        if (!e.empty() && e[0] != '.') e.insert(e.begin(), '.');
        const auto& list = Extensions();
        return std::find(list.begin(), list.end(), e) != list.end();
    }

    std::vector<std::string> ModelExtensions() { return Extensions(); }

    std::vector<ModelFile> ScanModelFiles(const std::string& rootDirectory, size_t maxFiles)
    {
        std::vector<ModelFile> files;
        std::error_code ec;
        if (rootDirectory.empty() || !fs::is_directory(rootDirectory, ec))
            return files;

        const fs::path root(rootDirectory);
        fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
        for (; !ec && it != end && files.size() < maxFiles; it.increment(ec))
        {
            std::error_code fileEc;
            if (!it->is_regular_file(fileEc) || fileEc) continue;
            if (!IsModelExtension(it->path().extension().string())) continue;

            ModelFile f;
            const fs::path relative = it->path().lexically_relative(root);
            f.path = Slashes(relative.generic_string());
            f.name = it->path().filename().string();
            f.folder = Slashes(relative.parent_path().generic_string());
            const auto size = it->file_size(fileEc);
            f.sizeBytes = fileEc ? 0 : static_cast<uint64_t>(size);
            files.push_back(std::move(f));
        }

        std::sort(files.begin(), files.end(), [](const ModelFile& a, const ModelFile& b)
        {
            // tree order: a folder, then its subfolders, then the next folder ("ship" before "ship/sails" before "ship parts"),
            // which a plain string compare gets wrong ('/' sorts after ' ')
            const std::vector<std::string> fa = Components(a.folder), fb = Components(b.folder);
            if (fa != fb) return fa < fb;
            return Lower(a.name) < Lower(b.name);
        });
        return files;
    }

    std::vector<size_t> FilterModelFiles(const std::vector<ModelFile>& files, const std::string& query)
    {
        std::vector<size_t> result;
        const std::string q = Lower(query);
        for (size_t i = 0; i < files.size(); ++i)
            if (q.empty() || Lower(files[i].path).find(q) != std::string::npos)
                result.push_back(i);
        return result;
    }

    std::string FileStem(const std::string& path)
    {
        return fs::path(path).stem().string();
    }

    std::string FormatBytes(uint64_t bytes)
    {
        static const char* kUnits[] = { "B", "KB", "MB", "GB", "TB" };
        double value = static_cast<double>(bytes);
        int unit = 0;
        while (value >= 1024.0 && unit < 4) { value /= 1024.0; ++unit; }
        char buffer[32];
        if (unit == 0) std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
        else           std::snprintf(buffer, sizeof(buffer), "%.1f %s", value, kUnits[unit]);
        return buffer;
    }

    std::string NormalizePath(const std::string& path)
    {
        std::error_code ec;
        fs::path p = fs::absolute(fs::path(path), ec);
        if (ec) p = fs::path(path);
        std::string s = Slashes(p.lexically_normal().generic_string());
#ifdef _WIN32
        s = Lower(s);                                       // Windows file names are case-insensitive
#endif
        return s;
    }
}
