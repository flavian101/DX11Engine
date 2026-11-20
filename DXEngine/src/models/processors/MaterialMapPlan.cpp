#include "MaterialMapPlan.h"
#include <algorithm>
#include <cctype>

namespace DXEngine
{
    namespace
    {
        std::vector<std::string> Tokens(const std::string& file)
        {
            // the file name only (no folders, no extension), lower case, split at every non-alphanumeric
            std::string name = file.substr(file.find_last_of("/\\") == std::string::npos ? 0 : file.find_last_of("/\\") + 1);
            const size_t dot = name.find_last_of('.');
            if (dot != std::string::npos) name.resize(dot);

            std::vector<std::string> tokens;
            std::string current;
            for (unsigned char c : name)
            {
                if (std::isalnum(c)) current.push_back(static_cast<char>(std::tolower(c)));
                else if (!current.empty()) { tokens.push_back(current); current.clear(); }
            }
            if (!current.empty()) tokens.push_back(current);
            return tokens;
        }

        bool HasToken(const std::string& file, std::initializer_list<const char*> wanted)
        {
            for (const std::string& t : Tokens(file))
                for (const char* w : wanted)
                    if (t == w) return true;
            return false;
        }

        // the same image referenced twice (different spellings of one path)
        bool SameFile(const std::string& a, const std::string& b)
        {
            if (a.empty() || b.empty()) return false;
            auto norm = [](std::string s)
            {
                std::replace(s.begin(), s.end(), '\\', '/');
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                return s;
            };
            return norm(a) == norm(b);
        }

        const std::string* Get(const std::map<SourceMap, std::string>& files, SourceMap kind)
        {
            auto it = files.find(kind);
            return (it == files.end() || it->second.empty()) ? nullptr : &it->second;
        }
    }

    bool NameHintsPackedData(const std::string& file) { return HasToken(file, { "orm", "arm", "rma", "mra" }); }
    bool NameHintsGloss(const std::string& file)      { return HasToken(file, { "gloss", "glossiness", "glossy", "smoothness", "smooth" }); }
    bool NameHintsRoughness(const std::string& file)  { return HasToken(file, { "rough", "roughness", "rgh" }); }
    bool NameHintsDirectX(const std::string& file)    { return HasToken(file, { "dx", "directx", "d3d" }); }
    bool NameHintsNormal(const std::string& file)     { return HasToken(file, { "normal", "normals", "nor", "nrm", "norm", "nrml" }); }

    const MapAssignment* MapPlan::Find(TextureSlot slot) const
    {
        for (const MapAssignment& m : maps) if (m.slot == slot) return &m;
        return nullptr;
    }

    MapPlan PlanMaps(const std::map<SourceMap, std::string>& files, bool fileGaveMetallicFactor, bool fileGaveRoughnessFactor)
    {
        MapPlan plan;
        auto add = [&](TextureSlot slot, SourceMap source, const std::string& file, int channel = 0)
        {
            plan.maps.push_back({ slot, source, file, channel });
        };

        // ---- colour and shape
        if (const std::string* f = Get(files, SourceMap::BaseColor))       add(TextureSlot::Diffuse, SourceMap::BaseColor, *f);
        else if (const std::string* f2 = Get(files, SourceMap::Diffuse))   add(TextureSlot::Diffuse, SourceMap::Diffuse, *f2);

        if (const std::string* f = Get(files, SourceMap::Normals))            add(TextureSlot::Normal, SourceMap::Normals, *f);
        else if (const std::string* f2 = Get(files, SourceMap::NormalCamera)) add(TextureSlot::Normal, SourceMap::NormalCamera, *f2);

        if (const std::string* f = Get(files, SourceMap::Emissive))           add(TextureSlot::Emissive, SourceMap::Emissive, *f);
        else if (const std::string* f2 = Get(files, SourceMap::EmissionColor)) add(TextureSlot::Emissive, SourceMap::EmissionColor, *f2);

        if (const std::string* f = Get(files, SourceMap::Height))     add(TextureSlot::Height, SourceMap::Height, *f);
        if (const std::string* f = Get(files, SourceMap::Opacity))    add(TextureSlot::Opacity, SourceMap::Opacity, *f);

        // ---- roughness and metallic
        const std::string* metal = Get(files, SourceMap::Metalness);
        const std::string* rough = Get(files, SourceMap::DiffuseRoughness);
        SourceMap roughSource = SourceMap::DiffuseRoughness;
        if (!rough)
        {
            // FBX exports of PBR materials call the roughness map "shininess"; only its NAME says whether it is roughness
            if (const std::string* shin = Get(files, SourceMap::Shininess))
            {
                if (NameHintsRoughness(*shin))
                {
                    rough = shin;
                    roughSource = SourceMap::Shininess;
                }
                else if (NameHintsGloss(*shin))
                    plan.warnings.push_back("'" + *shin + "' is a glossiness map (1 - roughness): not used, glossiness maps are not supported");
                else
                    plan.warnings.push_back("'" + *shin + "' is a shininess map and its name does not say whether it is roughness or glossiness: not used "
                                            "(a name with 'rough' in it is read as roughness)");
            }
        }

        // ---- occlusion
        const std::string* ao = Get(files, SourceMap::AmbientOcclusion);
        SourceMap aoSource = SourceMap::AmbientOcclusion;
        if (!ao) { ao = Get(files, SourceMap::Lightmap); aoSource = SourceMap::Lightmap; }    // glTF's occlusion texture
        if (!ao) { ao = Get(files, SourceMap::Ambient);  aoSource = SourceMap::Ambient; }     // FBX's ambient map

        if (metal && rough && SameFile(*metal, *rough))
        {
            // glTF: ONE image, roughness in green and metallic in blue. If the occlusion map is the same image it is a full
            // occlusion / roughness / metallic pack and occlusion is the red channel.
            add(TextureSlot::Roughness, roughSource, *rough, 1);
            add(TextureSlot::Metallic,  SourceMap::Metalness, *metal, 2);
            if (ao)
                add(TextureSlot::AmbientOcclusion, aoSource, *ao, 0);        // red: in the pack, or an image of its own
        }
        else
        {
            if (rough) add(TextureSlot::Roughness, roughSource, *rough, 0);
            if (metal) add(TextureSlot::Metallic,  SourceMap::Metalness, *metal, 0);
            if (ao)    add(TextureSlot::AmbientOcclusion, aoSource, *ao, 0);
        }

        // ---- specular: the engine's slot of that name is a PACKED data map, not a specular colour
        if (const std::string* spec = Get(files, SourceMap::Specular))
        {
            if (NameHintsPackedData(*spec))
                add(TextureSlot::Specular, SourceMap::Specular, *spec);
            else
                plan.warnings.push_back("'" + *spec + "' is a specular colour map: not used, the PBR shading has no specular colour "
                                        "(a packed occlusion / roughness / metallic map is recognised by 'orm' or 'arm' in its name)");
        }

        if (const MapAssignment* n = plan.Find(TextureSlot::Normal))
            plan.normalMapIsDirectX = NameHintsDirectX(n->file);

        // ---- a map multiplies its factor: with no factor in the file the factor must not be the engine's default
        if (plan.Find(TextureSlot::Metallic) && !fileGaveMetallicFactor)   plan.metallicFactorToOne = true;
        if (const MapAssignment* r = plan.Find(TextureSlot::Roughness))
        {
            // An FBX roughness map arrives as "shininess", and the importer derives a roughness FACTOR from the shininess
            // number (0.5 for the sample ship): that is not a multiplier the artist chose, so it must not darken the map.
            if (!fileGaveRoughnessFactor || r->source == SourceMap::Shininess)
                plan.roughnessFactorToOne = true;
        }
        return plan;
    }
}
