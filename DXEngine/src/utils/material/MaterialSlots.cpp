#include "MaterialSlots.h"
#include <cstring>

namespace DXEngine
{
    namespace MaterialSlots
    {
        namespace
        {
            // ----------------------------------------------------------------------------------------------
            // THE TABLE. One row per TextureSlot, in slot order (row i == slot i, register t<i>).
            // (A function-local static: safe to call from any other static initialiser.)
            // ----------------------------------------------------------------------------------------------
            const std::vector<SlotInfo>& Table()
            {
                static const std::vector<SlotInfo> kTable = {
                //  slot                          name                hlsl type      hlsl name              define                      colour space                 cube
                { TextureSlot::Diffuse,          "Diffuse",          "Texture2D",   "diffuseTexture",       "HAS_DIFFUSE_TEXTURE",      TextureColorSpace::SRGB,     false },
                { TextureSlot::Normal,           "Normal",           "Texture2D",   "normalTexture",        "HAS_NORMAL_MAP",           TextureColorSpace::Linear,   false },
                { TextureSlot::Specular,         "Specular",         "Texture2D",   "specularTexture",      "HAS_SPECULAR_MAP",         TextureColorSpace::Linear,   false },
                { TextureSlot::Emissive,         "Emissive",         "Texture2D",   "emissiveTexture",      "HAS_EMISSIVE_MAP",         TextureColorSpace::SRGB,     false },
                { TextureSlot::Roughness,        "Roughness",        "Texture2D",   "roughnessTexture",     "HAS_ROUGHNESS_MAP",        TextureColorSpace::Linear,   false },
                { TextureSlot::Metallic,         "Metallic",         "Texture2D",   "metallicTexture",      "HAS_METALLIC_MAP",         TextureColorSpace::Linear,   false },
                { TextureSlot::AmbientOcclusion, "AmbientOcclusion", "Texture2D",   "aoTexture",            "HAS_AO_MAP",               TextureColorSpace::Linear,   false },
                { TextureSlot::Height,           "Height",           "Texture2D",   "heightTexture",        "HAS_HEIGHT_MAP",           TextureColorSpace::Linear,   false },
                { TextureSlot::Opacity,          "Opacity",          "Texture2D",   "opacityTexture",       "HAS_OPACITY_MAP",          TextureColorSpace::Linear,   false },
                { TextureSlot::DetailDiffuse,    "DetailDiffuse",    "Texture2D",   "detailDiffuseTexture", "HAS_DETAIL_DIFFUSE_MAP",   TextureColorSpace::SRGB,     false },
                { TextureSlot::DetailNormal,     "DetailNormal",     "Texture2D",   "detailNormalTexture",  "HAS_DETAIL_NORMAL_MAP",    TextureColorSpace::Linear,   false },
                { TextureSlot::Environment,      "Environment",      "TextureCube", "environmentTexture",   "HAS_ENVIRONMENT_MAP",      TextureColorSpace::SRGB,     true  },
                { TextureSlot::Irradiance,       "Irradiance",       "TextureCube", "irradianceTexture",    "HAS_IRRADIANCE_MAP",       TextureColorSpace::SRGB,     true  },
                { TextureSlot::BRDF_LUT,         "BRDF_LUT",         "Texture2D",   "brdfLUTTexture",       "HAS_BRDF_LUT",             TextureColorSpace::Linear,   false },
                };
                return kTable;
            }
            static_assert(static_cast<uint32_t>(TextureSlot::BRDF_LUT) + 1 == kTextureSlotCount, "the last row must be the last slot");
        }

        const std::vector<SlotInfo>& All() { return Table(); }

        bool IsValid(TextureSlot slot) { return static_cast<uint32_t>(slot) < kTextureSlotCount; }

        const SlotInfo& Info(TextureSlot slot)
        {
            return Table()[IsValid(slot) ? static_cast<size_t>(slot) : 0];   // an invalid slot is a bug; do not read out of bounds
        }

        const char* Name(TextureSlot slot) { return IsValid(slot) ? Info(slot).name : "?"; }

        std::string GenerateTextureDefines(uint32_t mask)
        {
            std::string out;
            for (const SlotInfo& info : Table())
                if (Has(mask, info.slot))
                {
                    out += "#define ";
                    out += info.define;
                    out += " 1\n";
                }

            // derived switches (not slots, but fully decided by which slots are bound)
            if (Has(mask, TextureSlot::DetailDiffuse) || Has(mask, TextureSlot::DetailNormal))
                out += "#define HAS_DETAIL_TEXTURES 1\n";
            if (Has(mask, TextureSlot::Height))
                out += "#define ENABLE_PARALLAX_MAPPING 1\n";
            return out;
        }

        std::string GenerateHlslDeclarations()
        {
            std::string out;
            for (const SlotInfo& info : Table())
            {
                out += info.hlslType;
                out += ' ';
                out += info.hlslName;
                out += " : register(t";
                out += std::to_string(static_cast<uint32_t>(info.slot));
                out += ");   // TextureSlot::";
                out += info.name;
                out += '\n';
            }
            return out;
        }

        std::string MaskToString(uint32_t mask)
        {
            std::string out;
            for (const SlotInfo& info : Table())
                if (Has(mask, info.slot))
                {
                    if (!out.empty()) out += '+';
                    out += info.name;
                }
            return out.empty() ? "none" : out;
        }
    }
}
