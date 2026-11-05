#include "dxpch.h"
#include "TextureNaming.h"
#include <algorithm>

namespace DXEngine
{
    namespace TextureUtils
    {
        TextureType DetectTextureType(const std::string& filename)
        {
            std::string lower = filename;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

            // Diffuse/Albedo detection
            if (lower.find("diffuse") != std::string::npos ||
                lower.find("albedo") != std::string::npos ||
                lower.find("base") != std::string::npos ||
                lower.find("color") != std::string::npos ||
                lower.find("_d.") != std::string::npos ||
                lower.find("_diff.") != std::string::npos ||
                lower.find("_bc.") != std::string::npos)
            {
                return TextureType::Diffuse;
            }

            // Normal map detection
            if (lower.find("normal") != std::string::npos ||
                lower.find("nrm") != std::string::npos ||
                lower.find("_n.") != std::string::npos ||
                lower.find("_norm.") != std::string::npos ||
                lower.find("_nrml.") != std::string::npos ||
                lower.find("bump") != std::string::npos)
            {
                return TextureType::Normal;
            }

            // Roughness detection
            if (lower.find("roughness") != std::string::npos ||
                lower.find("rough") != std::string::npos ||
                lower.find("_r.") != std::string::npos ||
                lower.find("_rough.") != std::string::npos)
            {
                return TextureType::Roughness;
            }

            // Metallic detection
            if (lower.find("metallic") != std::string::npos ||
                lower.find("metal") != std::string::npos ||
                lower.find("_m.") != std::string::npos ||
                lower.find("_met.") != std::string::npos)
            {
                return TextureType::Metallic;
            }

            // Ambient Occlusion detection
            if (lower.find("ao") != std::string::npos ||
                lower.find("occlusion") != std::string::npos ||
                lower.find("ambient") != std::string::npos ||
                lower.find("_ao.") != std::string::npos)
            {
                return TextureType::AmbientOcclusion;
            }

            // Height/Displacement detection
            if (lower.find("height") != std::string::npos ||
                lower.find("displacement") != std::string::npos ||
                lower.find("disp") != std::string::npos ||
                lower.find("_h.") != std::string::npos ||
                lower.find("_height.") != std::string::npos)
            {
                return TextureType::Height;
            }

            // Emissive detection
            if (lower.find("emissive") != std::string::npos ||
                lower.find("emission") != std::string::npos ||
                lower.find("emit") != std::string::npos ||
                lower.find("_e.") != std::string::npos ||
                lower.find("glow") != std::string::npos)
            {
                return TextureType::Emissive;
            }

            // Opacity/Alpha detection
            if (lower.find("opacity") != std::string::npos ||
                lower.find("alpha") != std::string::npos ||
                lower.find("_a.") != std::string::npos ||
                lower.find("transparency") != std::string::npos)
            {
                return TextureType::Opacity;
            }

            // Specular detection (fallback for older workflows)
            if (lower.find("specular") != std::string::npos ||
                lower.find("spec") != std::string::npos ||
                lower.find("_s.") != std::string::npos)
            {
                return TextureType::Specular;
            }

            return TextureType::Unknown;
        }
    }
}
