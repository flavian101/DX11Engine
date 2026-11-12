#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace DXEngine
{
    // ==================================================================
    // THE texture slot table: the ONE place that says what a material texture slot is.
    //
    // A slot used to be written down five times (the TextureSlot enum, the ShaderFeature bit, a switch in
    // Material::SetTexture, the feature -> #define chain in ShaderCache, the `register(tN)` line in common.hlsli).
    // Every time one of them drifted, a shader sampled a texture that was never bound (patches 0022 and 0046).
    // Now there is a row per slot and everything else is DERIVED from it:
    //
    //   Material::SetTexture      -> the texture bit mask (bit i = a texture sits in slot i)
    //   ShaderCache               -> "#define HAS_..." lines from that mask      (GenerateTextureDefines)
    //   ShaderCache               -> the `Texture2D x : register(tN)` lines      (GenerateHlslDeclarations)
    //   MeshDrawer                -> binds texture i to register t<i>             (register == slot, always)
    //   ImageLoader / importers   -> sRGB or linear                               (SlotInfo::colorSpace)
    //
    // To add a slot: add the enumerator (before Count) and ONE row in MaterialSlots.cpp. Nothing else. The tests fail
    // if a row is missing, out of order, or two rows share a name, define or register.
    //
    // Pure logic (no GPU, no engine types): everything here is unit-tested.
    // ==================================================================

    // The value IS the HLSL register number (t0, t1, ...) the texture is bound to.
    enum class TextureSlot : uint32_t
    {
        // ---- surface ----
        Diffuse = 0,            // base colour / albedo
        Normal = 1,             // tangent-space normal
        Specular = 2,           // packed data: R = ambient occlusion, G = roughness, B = metallic (used when no dedicated maps)
        Emissive = 3,

        // ---- PBR parameters ----
        Roughness = 4,
        Metallic = 5,
        AmbientOcclusion = 6,
        Height = 7,             // parallax mapping

        // ---- extras ----
        Opacity = 8,
        DetailDiffuse = 9,
        DetailNormal = 10,

        // ---- image-based lighting ----
        Environment = 11,       // cube map: a sky, or reflections for PBR
        Irradiance = 12,        // cube map: diffuse IBL
        BRDF_LUT = 13,

        Count                   // number of slots; not a slot. (Shadow = 14 is gone: the shadow atlas is engine-wide, t35.)
    };

    constexpr uint32_t kTextureSlotCount = static_cast<uint32_t>(TextureSlot::Count);

    // Registers t0..t31 belong to the material textures; the engine's own resources (lights, clusters, shadows)
    // start above (SRV_Lights in ConstantBufferTypes.h). A slot can never grow into them.
    constexpr uint32_t kMaterialRegisterLimit = 32;
    static_assert(kTextureSlotCount <= kMaterialRegisterLimit, "more texture slots than material registers (t0..t31)");

    // How the texel values are meant to be read.
    //   SRGB   : colours (albedo, emissive). Stored gamma-encoded; the GPU converts to linear when sampling.
    //   Linear : data (normals, roughness, AO, ...). The numbers ARE the values; converting them would corrupt them.
    enum class TextureColorSpace : uint8_t { Linear, SRGB };

    struct SlotInfo
    {
        TextureSlot       slot;
        const char*       name;          // "Diffuse"    (logs, editors)
        const char*       hlslType;      // "Texture2D" | "TextureCube"
        const char*       hlslName;      // "diffuseTexture"   (the variable the sampling helpers in common.hlsli use)
        const char*       define;        // "HAS_DIFFUSE_TEXTURE": set when a texture is bound in this slot
        TextureColorSpace colorSpace;
        bool              isCube;
    };

    namespace MaterialSlots
    {
        const std::vector<SlotInfo>& All();                    // row i describes slot i (the tests check it)
        const SlotInfo&              Info(TextureSlot slot);   // slot must be < Count
        bool                         IsValid(TextureSlot slot);
        const char*                  Name(TextureSlot slot);   // "?" for an invalid slot

        // ---- the texture mask: bit i set = a texture is bound in slot i ----
        constexpr uint32_t MaskBit(TextureSlot slot) { return 1u << static_cast<uint32_t>(slot); }
        constexpr bool     Has(uint32_t mask, TextureSlot slot) { return (mask & MaskBit(slot)) != 0; }

        // "#define HAS_DIFFUSE_TEXTURE 1\n" for every set bit, in slot order, plus the two DERIVED switches:
        //   HAS_DETAIL_TEXTURES      when a detail diffuse or detail normal map is bound
        //   ENABLE_PARALLAX_MAPPING  when a height map is bound
        // Bits at or above Count are ignored. mask 0 -> "".
        std::string GenerateTextureDefines(uint32_t mask);

        // One line per slot, e.g. "Texture2D diffuseTexture : register(t0);". Prepended to every shader, so common.hlsli
        // no longer declares textures by hand and a register can never disagree with the C++ side.
        std::string GenerateHlslDeclarations();

        // Names of the set bits, "Diffuse+Normal" (stats, logs). mask 0 -> "none".
        std::string MaskToString(uint32_t mask);
    }
}
