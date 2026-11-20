#pragma once
// ============================================================================
// MaterialMapPlan: which texture file of an imported material feeds which slot of the engine's PBR material, in which
// channel. Pure logic (strings and enums): no assimp, no GPU, so it is tested without either (material_tests).
//
// Why it exists: the model formats disagree about where the PBR maps live, and the old importer read only some of the places.
//   * glTF packs metallic (blue) and roughness (green) in ONE image, and may share it with ambient occlusion (red);
//     assimp reports that one file as both the metalness and the roughness texture. The engine used to bind it to two slots
//     and read the red channel of both.
//   * glTF's occlusion map arrives as a "lightmap", FBX's often as an "ambient" map: both were dropped.
//   * FBX exports of PBR materials report the roughness map as "shininess": dropped.
//   * A classic specular colour map was put into the packed AO/roughness/metallic slot: wrong data in the wrong place.
// ============================================================================
#include "utils/material/MaterialSlots.h"
#include <map>
#include <string>
#include <vector>

namespace DXEngine
{
    // The texture "kinds" an importer can hand over (a neutral copy of the assimp texture types the engine cares about).
    enum class SourceMap
    {
        BaseColor, Diffuse, Normals, NormalCamera, Height, Specular, Shininess,
        Emissive, EmissionColor, Metalness, DiffuseRoughness, AmbientOcclusion, Lightmap, Ambient, Opacity
    };

    struct MapAssignment
    {
        TextureSlot slot = TextureSlot::Diffuse;
        SourceMap   source = SourceMap::Diffuse;   // which of the importer's kinds the file came from
        std::string file;                          // as the importer gave it
        int         channel = 0;                   // which channel carries the data: 0 R, 1 G, 2 B, 3 A (Roughness, Metallic, AO)
    };

    struct MapPlan
    {
        std::vector<MapAssignment> maps;
        std::vector<std::string>   warnings;       // maps that were NOT used, and why: an import that drops things says so

        // A map multiplies the material's factor. A file that gave no factor leaves the engine default (metallic 0), which
        // would silence the map: these ask for factor 1 instead (glTF's rule: no factor = 1 when there is a map). A roughness
        // map that came from FBX "shininess" also asks for 1: the factor assimp derived from that number is not a multiplier.
        bool normalMapIsDirectX = false;           // the normal map's file name says DirectX style ("..._nor_dx_...")

        bool metallicFactorToOne  = false;
        bool roughnessFactorToOne = false;

        const MapAssignment* Find(TextureSlot slot) const;
    };

    // 'files': the file of each kind the material has (a kind it does not have is absent). 'fileGave...Factor': the file
    // itself carried an explicit factor (glTF always does; FBX does not).
    MapPlan PlanMaps(const std::map<SourceMap, std::string>& files, bool fileGaveMetallicFactor, bool fileGaveRoughnessFactor);

    // ---- file name hints (lower case tokens split at anything that is not a letter or digit)
    bool NameHintsPackedData(const std::string& file);   // "x_orm.png", "x_arm.jpg": occlusion / roughness / metallic in R / G / B
    bool NameHintsGloss(const std::string& file);        // "x_gloss.png", "smoothness": 1 - roughness
    bool NameHintsRoughness(const std::string& file);    // "x_rough.exr", "roughness"
    bool NameHintsNormal(const std::string& file);       // "x_nor_gl.exr", "normal", "nrm"
    bool NameHintsDirectX(const std::string& file);      // "x_nor_dx.png", "directx": green channel points down
}
