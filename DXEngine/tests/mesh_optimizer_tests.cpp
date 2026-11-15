// MeshOptimizer: the engine's wrapper around meshoptimizer, tested against the real library. No GPU, no engine types.
#include "utils/Mesh/Utils/MeshOptimizer.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <set>

using namespace DXEngine;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)

using Tri = std::array<uint32_t, 3>;

// an n x n grid of quads (two triangles each), triangles in a deliberately bad (shuffled) order
static std::vector<uint32_t> ShuffledGrid(uint32_t n, unsigned seed)
{
    std::vector<Tri> tris;
    for (uint32_t y = 0; y < n; ++y)
        for (uint32_t x = 0; x < n; ++x)
        {
            const uint32_t a = y * (n + 1) + x, b = a + 1, c = a + (n + 1), d = c + 1;
            tris.push_back({ a, c, b });
            tris.push_back({ b, c, d });
        }
    std::mt19937 rng(seed);
    std::shuffle(tris.begin(), tris.end(), rng);
    std::vector<uint32_t> out;
    for (auto& t : tris) out.insert(out.end(), t.begin(), t.end());
    return out;
}

// the winding is part of a triangle, so a triangle is the exact ordered triple: the optimiser must keep every one
static std::multiset<Tri> Triangles(const uint32_t* idx, size_t count)
{
    std::multiset<Tri> s;
    for (size_t i = 0; i + 2 < count; i += 3) s.insert({ idx[i], idx[i + 1], idx[i + 2] });
    return s;
}
static std::multiset<Tri> Triangles(const std::vector<uint32_t>& idx) { return Triangles(idx.data(), idx.size()); }

static void KeepsEveryTriangleAndImprovesReuse()
{
    const uint32_t n = 64;
    std::vector<uint32_t> idx = ShuffledGrid(n, 7);
    const size_t vertices = (n + 1) * (n + 1);
    const auto before = Triangles(idx);
    const float acmrBefore = MeshOpt::AverageCacheMissRatio(idx.data(), idx.size(), vertices);

    CHECK(MeshOpt::OptimizeVertexCache(idx.data(), idx.size(), vertices));
    const float acmrAfter = MeshOpt::AverageCacheMissRatio(idx.data(), idx.size(), vertices);

    CHECK(Triangles(idx) == before);                 // same triangles, same winding: only the order changed
    CHECK(acmrBefore > 1.8f);                        // the shuffled grid really is bad
    CHECK(acmrAfter < 0.75f);                        // a grid can reach ~0.5
    std::printf("grid %ux%u: ACMR %.2f -> %.2f\n", n, n, acmrBefore, acmrAfter);
}

static void LeavesBadInputAlone()
{
    std::vector<uint32_t> two = { 0, 1 };
    CHECK(!MeshOpt::OptimizeVertexCache(two.data(), two.size(), 4));
    std::vector<uint32_t> notWhole = { 0, 1, 2, 3 };
    CHECK(!MeshOpt::OptimizeVertexCache(notWhole.data(), notWhole.size(), 4));
    std::vector<uint32_t> outOfRange = { 0, 1, 9 };
    const auto copy = outOfRange;
    CHECK(!MeshOpt::OptimizeVertexCache(outOfRange.data(), outOfRange.size(), 4));
    CHECK(outOfRange == copy);
    CHECK(!MeshOpt::OptimizeVertexCache(nullptr, 3, 4));

    std::vector<uint32_t> remap;
    CHECK(MeshOpt::BuildVertexFetchRemap(outOfRange.data(), outOfRange.size(), 4, remap) == 0);
    CHECK(outOfRange == copy && remap.empty());
}

static void OneTriangleAndDisconnectedPieces()
{
    std::vector<uint32_t> one = { 2, 0, 1 };
    CHECK(MeshOpt::OptimizeVertexCache(one.data(), one.size(), 3));
    CHECK(Triangles(one) == Triangles(std::vector<uint32_t>{ 2, 0, 1 }));

    std::vector<uint32_t> apart = { 0, 1, 2, 3, 4, 5 };
    CHECK(MeshOpt::OptimizeVertexCache(apart.data(), apart.size(), 6));
    CHECK(Triangles(apart) == Triangles(std::vector<uint32_t>{ 0, 1, 2, 3, 4, 5 }));
}

static void WorksOnARangeInsideALargerBuffer()
{
    // a submesh is a range of the index buffer: what is outside it must not move
    std::vector<uint32_t> idx = ShuffledGrid(8, 3);
    const std::vector<uint32_t> before = idx;
    const size_t first = 30, count = 60;             // whole triangles, not at the start
    CHECK(MeshOpt::OptimizeVertexCache(idx.data() + first, count, 9 * 9));
    for (size_t i = 0; i < idx.size(); ++i)
        if (i < first || i >= first + count) CHECK(idx[i] == before[i]);
    CHECK(Triangles(idx.data() + first, count) == Triangles(before.data() + first, count));
}

// Vertex fetch: vertices move into the order the triangles first use them. Every triangle must still reach the same
// POSITIONS (here: a position per vertex that is its old number), and unused vertices must disappear.
static void VertexFetchRemapKeepsEveryTriangleOnItsPositions()
{
    const uint32_t n = 32;
    std::vector<uint32_t> idx = ShuffledGrid(n, 5);
    const uint32_t used = (n + 1) * (n + 1);
    const uint32_t vertexCount = used + 10;          // ten vertices nobody uses

    std::vector<uint32_t> positionOf(vertexCount);   // old vertex number == its "position"
    for (uint32_t v = 0; v < vertexCount; ++v) positionOf[v] = v;

    std::vector<uint32_t> optimised = idx;
    std::vector<uint32_t> remap;
    const size_t remaining = MeshOpt::BuildVertexFetchRemap(optimised.data(), optimised.size(), vertexCount, remap);
    CHECK(remaining == used);                        // the ten unused ones are dropped
    CHECK(remap.size() == vertexCount);

    // apply the table to the vertex data the way VertexData::Remap does
    std::vector<uint32_t> moved(remaining);
    for (uint32_t old = 0; old < vertexCount; ++old)
        if (remap[old] != MeshOpt::kUnusedVertex) { CHECK(remap[old] < remaining); moved[remap[old]] = positionOf[old]; }
    for (uint32_t old = used; old < vertexCount; ++old) CHECK(remap[old] == MeshOpt::kUnusedVertex);

    for (size_t t = 0; t < idx.size(); t += 3)       // triangle t in the old and the new buffer: same three positions, same order
        for (size_t k = 0; k < 3; ++k)
            CHECK(moved[optimised[t + k]] == positionOf[idx[t + k]]);

    // the new numbering is the order of first use: 0, 1, 2, ... with no gaps
    uint32_t next = 0;
    for (uint32_t v : optimised)
        if (v == next) ++next;
        else CHECK(v < next);
    CHECK(next == remaining);
}

static void AnOptimisedMeshIsAFixedPointOfTheFetchPass()
{
    std::vector<uint32_t> idx = ShuffledGrid(16, 2);
    const size_t vertices = 17 * 17;
    MeshOpt::OptimizeVertexCache(idx.data(), idx.size(), vertices);
    std::vector<uint32_t> remap;
    MeshOpt::BuildVertexFetchRemap(idx.data(), idx.size(), vertices, remap);

    const std::vector<uint32_t> once = idx;
    std::vector<uint32_t> remap2;
    CHECK(MeshOpt::BuildVertexFetchRemap(idx.data(), idx.size(), vertices, remap2) == vertices);
    CHECK(idx == once);                              // a second pass changes nothing: the importer can run twice safely
    for (uint32_t v = 0; v < remap2.size(); ++v) CHECK(remap2[v] == v);
}

static void IsFastEnoughForALargeMesh()
{
    const uint32_t n = 700;                          // ~980,000 triangles
    std::vector<uint32_t> idx = ShuffledGrid(n, 11);
    const size_t vertices = (n + 1) * (n + 1);
    CHECK(MeshOpt::OptimizeVertexCache(idx.data(), idx.size(), vertices));
    std::vector<uint32_t> remap;
    CHECK(MeshOpt::BuildVertexFetchRemap(idx.data(), idx.size(), vertices, remap) == vertices);
    CHECK(MeshOpt::AverageCacheMissRatio(idx.data(), idx.size(), vertices) < 0.75f);
}

int main()
{
    KeepsEveryTriangleAndImprovesReuse();
    LeavesBadInputAlone();
    OneTriangleAndDisconnectedPieces();
    WorksOnARangeInsideALargerBuffer();
    VertexFetchRemapKeepsEveryTriangleOnItsPositions();
    AnOptimisedMeshIsAFixedPointOfTheFetchPass();
    IsFastEnoughForALargeMesh();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return EXIT_FAILURE; }
    std::printf("mesh_optimizer_tests: all passed\n");
    return EXIT_SUCCESS;
}
