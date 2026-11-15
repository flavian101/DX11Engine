// (no dxpch.h on purpose: this file needs nothing from the engine, so it also builds in the GPU-free tests)
#include "MeshOptimizer.h"
#include <meshoptimizer.h>
#include <algorithm>

namespace DXEngine::MeshOpt
{
    namespace
    {
        bool ValidTriangles(const uint32_t* indices, size_t indexCount, size_t vertexCount)
        {
            if (!indices || indexCount < 3 || indexCount % 3 != 0 || vertexCount == 0)
                return false;
            return std::all_of(indices, indices + indexCount, [vertexCount](uint32_t i) { return i < vertexCount; });
        }
    }

    bool OptimizeVertexCache(uint32_t* indices, size_t indexCount, size_t vertexCount)
    {
        if (!ValidTriangles(indices, indexCount, vertexCount))
            return false;
        // the library writes a new buffer: it does not promise to work in place
        std::vector<uint32_t> result(indexCount);
        meshopt_optimizeVertexCache(result.data(), indices, indexCount, vertexCount);
        std::copy(result.begin(), result.end(), indices);
        return true;
    }

    size_t BuildVertexFetchRemap(uint32_t* indices, size_t indexCount, size_t vertexCount, std::vector<uint32_t>& remap)
    {
        remap.clear();
        if (!ValidTriangles(indices, indexCount, vertexCount))
            return 0;
        remap.resize(vertexCount);
        const size_t unique = meshopt_optimizeVertexFetchRemap(remap.data(), indices, indexCount, vertexCount);
        std::vector<uint32_t> result(indexCount);
        meshopt_remapIndexBuffer(result.data(), indices, indexCount, remap.data());
        std::copy(result.begin(), result.end(), indices);
        return unique;
    }

    float AverageCacheMissRatio(const uint32_t* indices, size_t indexCount, size_t vertexCount, unsigned cacheSize)
    {
        if (!ValidTriangles(indices, indexCount, vertexCount))
            return 0.0f;
        return meshopt_analyzeVertexCache(indices, indexCount, vertexCount, cacheSize, 0, 0).acmr;
    }
}
