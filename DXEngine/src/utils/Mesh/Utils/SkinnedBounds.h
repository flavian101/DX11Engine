#pragma once
// ============================================================================
// SkinnedBounds: the bounding box of a SKINNED mesh. A skinned mesh is not where its vertex positions say: the bone matrices
// move, rotate and scale it (an imported model's bind pose already carries its node transform), so a box made from the raw
// positions is in the wrong place and the wrong size: culling hides the mesh while it is on screen, picking and the editor's
// selection box miss it. The box is made from the vertices AFTER skinning, over the poses it can take.
// Pure DirectXMath over plain arrays (tested natively: tests/skeleton_tests.cpp).
// ============================================================================
#include <DirectXMath.h>
#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <vector>

namespace DXEngine::SkinnedBounds
{
    struct Box
    {
        DirectX::XMFLOAT3 min = { FLT_MAX, FLT_MAX, FLT_MAX };
        DirectX::XMFLOAT3 max = { -FLT_MAX, -FLT_MAX, -FLT_MAX };
        bool valid() const { return min.x <= max.x; }
    };

    // 'positions', 'boneIndices' and 'boneWeights' have one entry per vertex (up to four influences each). A pose is the
    // bone matrices the animation evaluator produces: the GPU form (transposed: the shader multiplies column vectors).
    // A vertex without weights is not skinned and counts where it is; an influence naming a bone the pose does not have is skipped.
    // 'padding' grows the box by that fraction of its size: poses are sampled, not every frame is looked at.
    inline Box Compute(const std::vector<DirectX::XMFLOAT3>& positions,
                       const std::vector<DirectX::XMINT4>& boneIndices,
                       const std::vector<DirectX::XMFLOAT4>& boneWeights,
                       const std::vector<std::vector<DirectX::XMFLOAT4X4>>& poses,
                       float padding = 0.05f)
    {
        using namespace DirectX;
        Box box;
        const size_t count = std::min({ positions.size(), boneIndices.size(), boneWeights.size() });
        if (count == 0 || poses.empty())
            return box;

        // row-vector matrices once per pose
        std::vector<std::vector<XMMATRIX>> rows(poses.size());
        for (size_t p = 0; p < poses.size(); ++p)
        {
            rows[p].reserve(poses[p].size());
            for (const XMFLOAT4X4& m : poses[p])
                rows[p].push_back(XMMatrixTranspose(XMLoadFloat4x4(&m)));
        }

        for (size_t p = 0; p < poses.size(); ++p)
        {
            for (size_t v = 0; v < count; ++v)
            {
                const XMVECTOR position = XMVectorSet(positions[v].x, positions[v].y, positions[v].z, 1.0f);
                const int idx[4] = { boneIndices[v].x, boneIndices[v].y, boneIndices[v].z, boneIndices[v].w };
                const float w[4] = { boneWeights[v].x, boneWeights[v].y, boneWeights[v].z, boneWeights[v].w };

                XMVECTOR skinned = XMVectorZero();
                float total = 0.0f;
                for (int k = 0; k < 4; ++k)
                {
                    if (w[k] <= 0.0f || idx[k] < 0 || static_cast<size_t>(idx[k]) >= rows[p].size()) continue;
                    skinned = XMVectorAdd(skinned, XMVectorScale(XMVector3Transform(position, rows[p][idx[k]]), w[k]));
                    total += w[k];
                }
                if (total <= 1e-6f)
                    skinned = position;                                   // not skinned: where it is
                else if (total < 0.999f || total > 1.001f)
                    skinned = XMVectorScale(skinned, 1.0f / total);       // weights that do not add up to 1: normalise

                XMFLOAT3 q; XMStoreFloat3(&q, skinned);
                if (!(q.x == q.x && q.y == q.y && q.z == q.z)) continue;  // a NaN never reaches the box
                box.min = { std::min(box.min.x, q.x), std::min(box.min.y, q.y), std::min(box.min.z, q.z) };
                box.max = { std::max(box.max.x, q.x), std::max(box.max.y, q.y), std::max(box.max.z, q.z) };
            }
        }

        if (box.valid() && padding > 0.0f)
        {
            const float px = (box.max.x - box.min.x) * padding, py = (box.max.y - box.min.y) * padding, pz = (box.max.z - box.min.z) * padding;
            box.min = { box.min.x - px, box.min.y - py, box.min.z - pz };
            box.max = { box.max.x + px, box.max.y + py, box.max.z + pz };
        }
        return box;
    }
}
