#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace DXEngine
{
    // ==================================================================
    // AssetNames: the names scene files use for assets (see "Scene files" in docs/ROADMAP.md), as plain string functions: how
    // a name is made and how it is read back. Pure, no device, no files: all of it is tested without either.
    //
    //   primitive/cube?size=2                              a generated mesh (every parameter is in the name)
    //   primitive/sphere?radius=1&segments=32
    //   primitive/plane?width=10&depth=10&widthSegments=10&depthSegments=10
    //   primitive/quad?width=1&height=1
    //   model/models/horse.fbx#mesh/3                      a mesh / a material of an imported model (path, then index)
    //   model/models/horse.fbx#material/0
    //   cube:a.png|b.png|c.png|d.png|e.png|f.png           a cube map, one path per face
    //   textures/grass.jpg                                 any other name is a file path
    //
    // A name that has the right shape but silly numbers (a sphere with a million segments) is refused by Parse*: a scene file
    // is input and must not be able to make the engine allocate gigabytes.
    // ==================================================================
    namespace AssetNames
    {
        // ---- generated meshes
        struct Primitive
        {
            enum class Kind { Invalid, Quad, Cube, Sphere, Plane };
            Kind     kind = Kind::Invalid;
            float    a = 0.0f, b = 0.0f;           // quad: width, height. cube: size. sphere: radius. plane: width, depth
            uint32_t n = 0, m = 0;                 // sphere: segments. plane: width segments, depth segments
        };

        constexpr uint32_t kMaxSegments = 1024;     // per side; a plane is also limited to 1M cells

        std::string QuadName(float width, float height);
        std::string CubeName(float size);
        std::string SphereName(float radius, uint32_t segments);
        std::string PlaneName(float width, float depth, uint32_t widthSegments, uint32_t depthSegments);
        bool ParsePrimitive(const std::string& name, Primitive& out);   // nothing is written on failure

        // ---- parts of an imported model
        enum class ModelPart { Mesh, Material };
        std::string ModelPartName(const std::string& modelPath, ModelPart part, size_t index);
        bool ParseModelPart(const std::string& name, std::string& modelPath, ModelPart& part, size_t& index);

        // ---- cube maps (six face files)
        std::string CubeMapName(const std::array<std::string, 6>& faces);
        bool ParseCubeMapName(const std::string& name, std::array<std::string, 6>& faces);   // no face may be empty

        // ---- file paths inside a name
        // Stored form: '/' separators, relative to 'root' when the file is under it (so a project moves between machines),
        // otherwise the absolute path. Stored -> usable: a relative name is looked up under 'root'.
        // (std::filesystem's lexical rules only: nothing touches the disk. 'root' empty = names are used as they are.)
        std::string ToStoredPath(const std::string& path, const std::string& root);
        std::string FromStoredPath(const std::string& stored, const std::string& root);
    }
}
