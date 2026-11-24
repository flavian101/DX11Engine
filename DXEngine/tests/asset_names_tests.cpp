// AssetNames: how scene files name assets, and how a name is read back. Pure functions.
#include "utils/AssetNames.h"
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <limits>

using namespace DXEngine;
using namespace DXEngine::AssetNames;

static int g_failed = 0;
#define CHECK(c) do { if (!(c)) { std::printf("  FAILED line %d: %s\n", __LINE__, #c); ++g_failed; } } while (0)

static void TestPrimitiveRoundTrip()
{
    Primitive p;
    CHECK(CubeName(2.0f) == "primitive/cube?size=2");                 // short and readable
    CHECK(ParsePrimitive(CubeName(2.0f), p) && p.kind == Primitive::Kind::Cube && p.a == 2.0f);
    CHECK(ParsePrimitive(SphereName(0.1f, 32), p) && p.kind == Primitive::Kind::Sphere && p.a == 0.1f && p.n == 32);   // 0.1f exactly
    CHECK(SphereName(0.1f, 32) == "primitive/sphere?radius=0.1&segments=32");
    CHECK(ParsePrimitive(PlaneName(10.0f, 7.5f, 10, 4), p) && p.kind == Primitive::Kind::Plane && p.a == 10.0f && p.b == 7.5f && p.n == 10 && p.m == 4);
    CHECK(ParsePrimitive(QuadName(1.0f, 3.0f), p) && p.kind == Primitive::Kind::Quad && p.a == 1.0f && p.b == 3.0f);

    const float awkward[] = { 1.0f / 3.0f, 123456.789f, 1e-5f, 0.001f, 999999.0f };
    for (float v : awkward)
    {
        CHECK(ParsePrimitive(CubeName(v), p) && p.a == v);            // every float survives the text
        CHECK(ParsePrimitive(SphereName(v, 8), p) && p.a == v);
    }
    CHECK(ParsePrimitive("primitive/plane?depthSegments=4&widthSegments=10&depth=7.5&width=10", p) && p.n == 10 && p.m == 4);   // any order
}

static void TestPrimitiveRejects()
{
    Primitive p; p.kind = Primitive::Kind::Cube; p.a = 42.0f;
    const char* bad[] = {
        "", "cube", "primitive/", "primitive/cube", "primitive/cube?", "primitive/cube?size", "primitive/cube?size=", "primitive/cube?size=abc",
        "primitive/cube?size=2x", "primitive/cube?size=0", "primitive/cube?size=-1", "primitive/cube?size=nan", "primitive/cube?size=inf", "primitive/cube?size=1e30",
        "primitive/cube?size=2&size=3", "primitive/cube?size=2&extra=1", "primitive/cube?side=2", "primitive/cube?size=2&", "primitive/cube?&size=2",
        "primitive/torus?size=2", "Primitive/cube?size=2", "primitive/cube?=2",
        "primitive/sphere?radius=1", "primitive/sphere?radius=1&segments=2", "primitive/sphere?radius=1&segments=100000", "primitive/sphere?radius=1&segments=-3",
        "primitive/sphere?radius=1&segments=3.5", "primitive/sphere?radius=1&segments=4294967296",
        "primitive/plane?width=1&depth=1&widthSegments=0&depthSegments=1", "primitive/plane?width=1&depth=1&widthSegments=1025&depthSegments=1",
        "primitive/plane?width=1&depth=1&widthSegments=1024&depthSegments=1025", "primitive/plane?width=1&depth=1&widthSegments=2",
        "primitive/quad?width=1", "primitive/quad?width=1&height=0",
    };
    for (const char* name : bad)
    {
        if (ParsePrimitive(name, p)) std::printf("  accepted: %s\n", name);
        CHECK(!ParsePrimitive(name, p));
    }
    CHECK(p.kind == Primitive::Kind::Cube && p.a == 42.0f);           // nothing is written on failure
    CHECK(ParsePrimitive("primitive/plane?width=1&depth=1&widthSegments=1024&depthSegments=1024", p));   // the limit itself is fine (1M cells)
    CHECK(!ParsePrimitive("primitive/sphere?radius=1&segments=1025", p) && ParsePrimitive("primitive/sphere?radius=1&segments=1024", p));
}

static void TestModelParts()
{
    std::string path; ModelPart part; size_t index;
    const std::string mesh = ModelPartName("models/horse.fbx", ModelPart::Mesh, 3);
    CHECK(mesh == "model/models/horse.fbx#mesh/3");
    CHECK(ParseModelPart(mesh, path, part, index) && path == "models/horse.fbx" && part == ModelPart::Mesh && index == 3);
    CHECK(ParseModelPart(ModelPartName("a/b.glb", ModelPart::Material, 0), path, part, index) && part == ModelPart::Material && index == 0 && path == "a/b.glb");
    CHECK(ModelPartName("models\\horse.fbx", ModelPart::Mesh, 1) == "model/models/horse.fbx#mesh/1");     // '/' in names
    CHECK(ParseModelPart(ModelPartName("weird#folder/x.fbx", ModelPart::Mesh, 2), path, part, index) && path == "weird#folder/x.fbx" && index == 2);   // '#' in a path: the last one counts

    path = "unchanged"; index = 99;
    const char* bad[] = { "", "model/", "model/x.fbx", "model/x.fbx#", "model/x.fbx#mesh", "model/x.fbx#mesh/", "model/x.fbx#mesh/-1", "model/x.fbx#mesh/a",
                          "model/x.fbx#node/1", "model/#mesh/1", "models/x.fbx#mesh/1", "model/x.fbx#mesh/1 ", "model/x.fbx#mesh/99999999999" };
    for (const char* name : bad)
    {
        if (ParseModelPart(name, path, part, index)) std::printf("  accepted: %s\n", name);
        CHECK(!ParseModelPart(name, path, part, index));
    }
    CHECK(path == "unchanged" && index == 99);
}

static void TestCubeMaps()
{
    const std::array<std::string, 6> faces = { "sky/px.png", "sky/nx.png", "sky/py.png", "sky/ny.png", "sky/pz.png", "sky\\nz.png" };
    const std::string name = CubeMapName(faces);
    CHECK(name == "cube:sky/px.png|sky/nx.png|sky/py.png|sky/ny.png|sky/pz.png|sky/nz.png");
    std::array<std::string, 6> back;
    CHECK(ParseCubeMapName(name, back) && back[0] == "sky/px.png" && back[5] == "sky/nz.png");
    CHECK(!ParseCubeMapName("cube:a|b|c|d|e", back));                  // five
    CHECK(!ParseCubeMapName("cube:a|b|c|d|e|f|g", back));              // seven
    CHECK(!ParseCubeMapName("cube:a||c|d|e|f", back) && !ParseCubeMapName("cube:|b|c|d|e|f", back) && !ParseCubeMapName("cube:a|b|c|d|e|", back));
    CHECK(!ParseCubeMapName("sky/px.png", back) && !ParseCubeMapName("", back) && !ParseCubeMapName("cube:", back));
    CHECK(back[0] == "sky/px.png");                                    // untouched by the failures
}

static void TestPaths()
{
    CHECK(ToStoredPath("/proj/textures/grass.jpg", "/proj") == "textures/grass.jpg");        // under the root: relative
    CHECK(ToStoredPath("/proj/a/../textures/./grass.jpg", "/proj") == "textures/grass.jpg"); // normalised
    CHECK(ToStoredPath("/proj", "/proj").empty() || ToStoredPath("/proj", "/proj") == ".");   // the root itself: nothing useful, never ".."
    CHECK(ToStoredPath("/elsewhere/x.png", "/proj") == "/elsewhere/x.png");                 // outside: absolute
    CHECK(ToStoredPath("/proj2/x.png", "/proj") == "/proj2/x.png");                          // a sibling folder with the same prefix is NOT inside
    CHECK(ToStoredPath("textures\\grass.jpg", "/proj") == "textures/grass.jpg");             // relative and backslashes: kept, as '/'
    CHECK(ToStoredPath("assets/textures/grass.jpg", "") == "assets/textures/grass.jpg");     // no root: as it is
    CHECK(ToStoredPath("", "/proj").empty());

    CHECK(FromStoredPath("textures/grass.jpg", "/proj") == "/proj/textures/grass.jpg");
    CHECK(FromStoredPath("/elsewhere/x.png", "/proj") == "/elsewhere/x.png");                // absolute stays
    CHECK(FromStoredPath("textures/grass.jpg", "") == "textures/grass.jpg");
    CHECK(FromStoredPath("../up.png", "/proj/sub") == "/proj/up.png");
    CHECK(FromStoredPath("", "/proj").empty());
    // a round trip lands on the same file
    CHECK(FromStoredPath(ToStoredPath("/proj/models/h.fbx", "/proj"), "/proj") == "/proj/models/h.fbx");
    CHECK(FromStoredPath(ToStoredPath("/elsewhere/h.fbx", "/proj"), "/proj") == "/elsewhere/h.fbx");
}

int main()
{
    struct Test { const char* name; void (*fn)(); };
    const Test tests[] = {
        { "primitive names round trip exactly",      TestPrimitiveRoundTrip },
        { "silly primitive names are refused",       TestPrimitiveRejects },
        { "model part names",                        TestModelParts },
        { "cube map names",                          TestCubeMaps },
        { "paths relative to a root",                TestPaths },
    };
    for (const Test& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll asset name tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
