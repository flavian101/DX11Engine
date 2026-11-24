#include "dxpch.h"
#include "SceneIO.h"
#include <filesystem>
#include <fstream>
#include <system_error>

namespace DXEngine
{
    namespace SceneIO
    {
        namespace fs = std::filesystem;

        IoResult WriteTextFile(const std::string& path, const std::string& text)
        {
            IoResult r;
            if (path.empty()) { r.error = "no file name"; return r; }

            const fs::path target(path);
            const fs::path temp = fs::path(path + ".tmp");
            std::error_code ec;
            if (target.has_parent_path())
            {
                fs::create_directories(target.parent_path(), ec);      // "assets/scenes/new.dxscene" in a folder that does not exist yet
                if (ec) { r.error = "cannot create the folder '" + target.parent_path().string() + "': " + ec.message(); return r; }
            }

            {
                std::ofstream out(temp, std::ios::binary | std::ios::trunc);
                if (!out) { r.error = "cannot write '" + temp.string() + "'"; return r; }
                out.write(text.data(), static_cast<std::streamsize>(text.size()));
                out.flush();
                if (!out) { out.close(); fs::remove(temp, ec); r.error = "writing '" + temp.string() + "' failed (is the disk full?)"; return r; }
            }

            fs::rename(temp, target, ec);                               // replaces an existing file
            if (ec)
            {
                std::error_code ignore;
                fs::remove(temp, ignore);
                r.error = "cannot replace '" + path + "': " + ec.message() + " (is it open in another program?)";
                return r;
            }
            r.ok = true;
            return r;
        }

        IoResult ReadTextFile(const std::string& path, std::string& text)
        {
            IoResult r;
            text.clear();
            std::error_code ec;
            const fs::path file(path);
            if (!fs::is_regular_file(file, ec)) { r.error = "'" + path + "' is not a file"; return r; }
            const uintmax_t size = fs::file_size(file, ec);
            if (ec) { r.error = "cannot read the size of '" + path + "': " + ec.message(); return r; }
            if (size > kMaxFileBytes) { r.error = "'" + path + "' is " + std::to_string(size / (1024 * 1024)) + " MB: not a scene file"; return r; }

            std::ifstream in(file, std::ios::binary);
            if (!in) { r.error = "cannot open '" + path + "'"; return r; }
            std::string data(static_cast<size_t>(size), '\0');
            in.read(data.data(), static_cast<std::streamsize>(size));
            if (static_cast<uintmax_t>(in.gcount()) != size) { r.error = "reading '" + path + "' stopped early"; return r; }
            text = std::move(data);
            r.ok = true;
            return r;
        }

        std::string WithSceneExtension(const std::string& path)
        {
            if (path.empty()) return path;
            const fs::path p(path);
            return p.has_extension() ? path : path + kExtension;
        }
    }
}
