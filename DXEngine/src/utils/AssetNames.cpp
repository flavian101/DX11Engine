#include "dxpch.h"
#include "AssetNames.h"
#include <charconv>
#include <cmath>
#include <filesystem>
#include <map>

namespace DXEngine
{
    namespace AssetNames
    {
        namespace
        {
            // the shortest text that reads back as the very same float
            std::string Num(float v)
            {
                char buf[32];
                const auto r = std::to_chars(buf, buf + sizeof(buf), v);
                return std::string(buf, r.ptr);
            }

            bool ParseFloat(const std::string& s, float& out)
            {
                if (s.empty()) return false;
                float v = 0.0f;
                const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
                if (r.ec != std::errc() || r.ptr != s.data() + s.size() || !std::isfinite(v)) return false;
                out = v;
                return true;
            }

            bool ParseUInt(const std::string& s, uint32_t& out)
            {
                if (s.empty()) return false;
                uint32_t v = 0;
                const auto r = std::from_chars(s.data(), s.data() + s.size(), v);
                if (r.ec != std::errc() || r.ptr != s.data() + s.size()) return false;
                out = v;
                return true;
            }

            // "a=1&b=2" -> {a:1, b:2}; false for anything else (an empty key, a missing '=', a repeated key)
            bool ParseQuery(const std::string& q, std::map<std::string, std::string>& out)
            {
                size_t pos = 0;
                while (pos <= q.size())
                {
                    size_t amp = q.find('&', pos);
                    if (amp == std::string::npos) amp = q.size();
                    const std::string item = q.substr(pos, amp - pos);
                    const size_t eq = item.find('=');
                    if (eq == std::string::npos || eq == 0) return false;
                    if (!out.emplace(item.substr(0, eq), item.substr(eq + 1)).second) return false;
                    pos = amp + 1;
                    if (amp == q.size()) break;
                }
                return !out.empty();
            }

            bool Size(float v) { return v > 0.0f && v <= 1.0e6f; }

            std::string Slashes(std::string s)
            {
                for (char& c : s) if (c == '\\') c = '/';
                return s;
            }
        }

        // ---------------------------------------------------------------- generated meshes
        std::string QuadName(float width, float height)   { return "primitive/quad?width=" + Num(width) + "&height=" + Num(height); }
        std::string CubeName(float size)                  { return "primitive/cube?size=" + Num(size); }
        std::string SphereName(float radius, uint32_t segments) { return "primitive/sphere?radius=" + Num(radius) + "&segments=" + std::to_string(segments); }
        std::string PlaneName(float width, float depth, uint32_t ws, uint32_t ds)
        {
            return "primitive/plane?width=" + Num(width) + "&depth=" + Num(depth) + "&widthSegments=" + std::to_string(ws) + "&depthSegments=" + std::to_string(ds);
        }

        bool ParsePrimitive(const std::string& name, Primitive& out)
        {
            static const std::string kPrefix = "primitive/";
            if (name.compare(0, kPrefix.size(), kPrefix) != 0) return false;
            const size_t q = name.find('?');
            if (q == std::string::npos) return false;
            const std::string kind = name.substr(kPrefix.size(), q - kPrefix.size());
            std::map<std::string, std::string> args;
            if (!ParseQuery(name.substr(q + 1), args)) return false;

            Primitive p;
            const auto f = [&](const char* key, float& dst) { const auto it = args.find(key); return it != args.end() && ParseFloat(it->second, dst); };
            const auto u = [&](const char* key, uint32_t& dst) { const auto it = args.find(key); return it != args.end() && ParseUInt(it->second, dst); };
            const auto segs = [](uint32_t v) { return v >= 1 && v <= kMaxSegments; };

            if (kind == "cube")
            {
                if (args.size() != 1 || !f("size", p.a) || !Size(p.a)) return false;
                p.kind = Primitive::Kind::Cube;
            }
            else if (kind == "quad")
            {
                if (args.size() != 2 || !f("width", p.a) || !f("height", p.b) || !Size(p.a) || !Size(p.b)) return false;
                p.kind = Primitive::Kind::Quad;
            }
            else if (kind == "sphere")
            {
                if (args.size() != 2 || !f("radius", p.a) || !u("segments", p.n) || !Size(p.a) || p.n < 3 || p.n > kMaxSegments) return false;
                p.kind = Primitive::Kind::Sphere;
            }
            else if (kind == "plane")
            {
                if (args.size() != 4 || !f("width", p.a) || !f("depth", p.b) || !u("widthSegments", p.n) || !u("depthSegments", p.m)) return false;
                if (!Size(p.a) || !Size(p.b) || !segs(p.n) || !segs(p.m) || static_cast<uint64_t>(p.n) * p.m > 1024u * 1024u) return false;
                p.kind = Primitive::Kind::Plane;
            }
            else return false;
            out = p;
            return true;
        }

        // ---------------------------------------------------------------- model parts
        std::string ModelPartName(const std::string& modelPath, ModelPart part, size_t index)
        {
            return "model/" + Slashes(modelPath) + (part == ModelPart::Mesh ? "#mesh/" : "#material/") + std::to_string(index);
        }

        bool ParseModelPart(const std::string& name, std::string& modelPath, ModelPart& part, size_t& index)
        {
            static const std::string kPrefix = "model/";
            if (name.compare(0, kPrefix.size(), kPrefix) != 0) return false;
            const size_t hash = name.rfind('#');                // paths may contain '#'; the LAST one starts the part
            if (hash == std::string::npos || hash <= kPrefix.size()) return false;
            const std::string path = name.substr(kPrefix.size(), hash - kPrefix.size());
            const std::string tail = name.substr(hash + 1);
            ModelPart p;
            std::string number;
            if (tail.compare(0, 5, "mesh/") == 0)          { p = ModelPart::Mesh;     number = tail.substr(5); }
            else if (tail.compare(0, 9, "material/") == 0) { p = ModelPart::Material; number = tail.substr(9); }
            else return false;
            uint32_t i;
            if (!ParseUInt(number, i)) return false;
            modelPath = path;
            part = p;
            index = i;
            return true;
        }

        // ---------------------------------------------------------------- cube maps
        std::string CubeMapName(const std::array<std::string, 6>& faces)
        {
            std::string s = "cube:";
            for (size_t i = 0; i < 6; ++i) { if (i) s += '|'; s += Slashes(faces[i]); }
            return s;
        }

        bool ParseCubeMapName(const std::string& name, std::array<std::string, 6>& faces)
        {
            static const std::string kPrefix = "cube:";
            if (name.compare(0, kPrefix.size(), kPrefix) != 0) return false;
            std::array<std::string, 6> out;
            size_t pos = kPrefix.size();
            for (size_t i = 0; i < 6; ++i)
            {
                size_t bar = name.find('|', pos);
                if (i < 5 && bar == std::string::npos) return false;
                if (i == 5) bar = name.size();
                out[i] = name.substr(pos, bar - pos);
                if (out[i].empty() || out[i].find('|') != std::string::npos) return false;
                pos = bar + 1;
            }
            faces = std::move(out);
            return true;
        }

        // ---------------------------------------------------------------- paths
        std::string ToStoredPath(const std::string& path, const std::string& root)
        {
            namespace fs = std::filesystem;
            if (path.empty()) return {};
            fs::path p = fs::path(Slashes(path)).lexically_normal();
            if (root.empty()) return p.generic_string();
            fs::path r = fs::path(Slashes(root)).lexically_normal();
            if (p.is_relative()) return p.generic_string();     // already "from the root": the file's own business
            if (r.is_relative()) r = fs::absolute(r).lexically_normal();
            const fs::path rel = p.lexically_relative(r);
            if (!rel.empty() && *rel.begin() != "..") return rel.generic_string();
            return p.generic_string();                          // outside the root: absolute
        }

        std::string FromStoredPath(const std::string& stored, const std::string& root)
        {
            namespace fs = std::filesystem;
            if (stored.empty()) return {};
            const fs::path s(Slashes(stored));
            if (root.empty() || s.is_absolute()) return s.generic_string();
            return (fs::path(Slashes(root)) / s).lexically_normal().generic_string();
        }
    }
}
