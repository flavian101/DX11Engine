#pragma once
#include <string>

namespace DXEngine
{
    // Texture "kind" guessed from a file name (albedo / normal / roughness ...).
    // Moved verbatim out of the legacy Texture class so the model importer can keep using it
    // after the legacy D3D11 Texture / CubeMapTexture classes are removed.
    enum class TextureType
    {
        Unknown,
        Diffuse,
        Normal,
        Specular,
        Roughness,
        Metallic,
        AmbientOcclusion,
        Height,
        Emissive,
        Opacity,
        DetailMask,
        Subsurface,
        Anisotropy,
        Clearcoat,
        Environment
    };

    namespace TextureUtils
    {
        // Order matters: the first matching pattern wins (for example "bump" is a Normal map).
        TextureType DetectTextureType(const std::string& filename);
    }
}
