#pragma once
// ============================================================================
// MeshOptimizer: the engine's one door to the meshoptimizer library (vendor/meshoptimizer, zeux/meshoptimizer).
// Plain integers and vectors only: no GPU, no engine types, so it is tested natively (tests/mesh_optimizer_tests.cpp)
// against the real library. Nothing else in the engine includes <meshoptimizer.h>.
//
// Two things are done to a mesh at import time:
//   1. VERTEX CACHE: reorder the TRIANGLES of a draw range so neighbouring triangles reuse vertices the GPU just
//      transformed. Only the triangle order changes; every triangle keeps its own three indices (and so its winding).
//   2. VERTEX FETCH: reorder the VERTICES in memory in the order the (already reordered) triangles first use them, so the
//      GPU reads vertex memory front to back. Unused vertices are dropped.
// ============================================================================
#include <cstddef>
#include <cstdint>
#include <vector>

namespace DXEngine::MeshOpt
{
    constexpr uint32_t kUnusedVertex = ~0u;      // a remap entry for a vertex no triangle uses

    // Reorders the triangles of indices[0 .. indexCount) in place. A draw range of a bigger index buffer is optimised
    // on its own (pass a pointer into the buffer), so triangles never cross a submesh.
    // Returns false, and changes nothing, when the range is not whole triangles or an index is >= vertexCount.
    bool OptimizeVertexCache(uint32_t* indices, size_t indexCount, size_t vertexCount);

    // Plans the vertex fetch reorder for a whole index buffer and rewrites 'indices' to the new vertex numbers.
    // 'remap' (size vertexCount) maps old vertex -> new vertex, or kUnusedVertex for an unreferenced vertex.
    // Returns the number of vertices that remain (0 and nothing changed when the input is invalid).
    size_t BuildVertexFetchRemap(uint32_t* indices, size_t indexCount, size_t vertexCount, std::vector<uint32_t>& remap);

    // Average cache miss ratio (transformed vertices per triangle) for a FIFO cache of 'cacheSize'. 3.0 = nothing is
    // reused, ~0.5 = the best a closed mesh can do.
    float AverageCacheMissRatio(const uint32_t* indices, size_t indexCount, size_t vertexCount, unsigned cacheSize = 16);
}
