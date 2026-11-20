// The skeleton evaluator and the transforms ABOVE a root bone. Pure DirectXMath: no GPU, no engine sources.
//
// An imported skinned mesh sits under nodes that rotate and scale it (an FBX "Armature": rotated 90 degrees, scaled 100). The
// offset (inverse bind) matrices are relative to a bone placed WITH those nodes, so the root bone's world transform must
// include them (Bone::RootParentTransform); without it the mesh came out rotated and scaled wrongly.
#include "Animation/AnimationClip.h"
#include "Animation/AnimationEvaluator.h"
#include "utils/Mesh/Utils/SkinnedBounds.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace DXEngine;
using namespace DirectX;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
static bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
static bool NearV(FXMVECTOR a, FXMVECTOR b, float eps = 1e-3f) { return XMVector3NearEqual(a, b, XMVectorReplicate(eps)); }

static XMFLOAT4X4 Store(CXMMATRIX m) { XMFLOAT4X4 f; XMStoreFloat4x4(&f, m); return f; }

struct Rig
{
    Skeleton skeleton;
    XMMATRIX ancestors;        // the non-bone nodes above the root bone
    XMMATRIX meshGlobal;       // the mesh node's world transform (what a static mesh there would get)
};

// root bone (0, 1, 0) above the origin, one child bone (0, 1, 0) above that
static Rig MakeRig(bool includeAncestorsInSkeleton)
{
    Rig rig;
    rig.ancestors = XMMatrixRotationX(XM_PIDIV2) * XMMatrixScaling(10, 10, 10);     // row vectors: rotate, then scale
    rig.meshGlobal = rig.ancestors;                                                 // the mesh hangs under the same nodes

    Bone root; root.Name = "root"; root.ParentIndex = -1;
    root.LocalTransform = Store(XMMatrixTranslation(0, 1, 0));
    Bone child; child.Name = "child"; child.ParentIndex = 0;
    child.LocalTransform = Store(XMMatrixTranslation(0, 1, 0));
    if (includeAncestorsInSkeleton) root.RootParentTransform = Store(rig.ancestors);

    // bind-pose globals WITH the ancestors; assimp's offset: mesh space -> bone space (row-vector form: M * G^-1)
    const XMMATRIX gRoot = XMMatrixTranslation(0, 1, 0) * rig.ancestors;
    const XMMATRIX gChild = XMMatrixTranslation(0, 1, 0) * gRoot;
    root.OffsetMatrix  = Store(rig.meshGlobal * XMMatrixInverse(nullptr, gRoot));
    child.OffsetMatrix = Store(rig.meshGlobal * XMMatrixInverse(nullptr, gChild));
    rig.skeleton.AddBone(root);
    rig.skeleton.AddBone(child);
    return rig;
}

// The evaluator hands the GPU TRANSPOSED matrices (the shader multiplies column vectors): back to row-vector form.
static XMMATRIX RowForm(const XMFLOAT4X4& stored) { return XMMatrixTranspose(XMLoadFloat4x4(&stored)); }

static void BindPoseEqualsTheMeshNodesTransform()
{
    Rig rig = MakeRig(true);
    AnimationClip bind("bind", 1.0f);                               // no channels: every bone stays at its local transform
    AnimationEvaluator evaluator;
    std::vector<XMFLOAT4X4> out;
    evaluator.Evaluate(bind, rig.skeleton, 0.0f, out);
    CHECK(out.size() == 2);

    // at bind, a vertex ends up exactly where a STATIC mesh on the same node would: v * meshGlobal
    const XMVECTOR v = XMVectorSet(0.3f, -0.2f, 0.7f, 1.0f);
    const XMVECTOR expected = XMVector3Transform(v, rig.meshGlobal);
    for (size_t b = 0; b < out.size(); ++b)
        CHECK(NearV(XMVector3Transform(v, RowForm(out[b])), expected));
}

static void WithoutTheAncestorsTheMeshIsWrong()
{
    // the old behaviour: the nodes above the root bone were dropped from the skeleton
    Rig rig = MakeRig(false);
    AnimationClip bind("bind", 1.0f);
    AnimationEvaluator evaluator;
    std::vector<XMFLOAT4X4> out;
    evaluator.Evaluate(bind, rig.skeleton, 0.0f, out);
    const XMVECTOR v = XMVectorSet(0.3f, -0.2f, 0.7f, 1.0f);
    CHECK(!NearV(XMVector3Transform(v, RowForm(out[0])), XMVector3Transform(v, rig.meshGlobal), 0.5f));   // far from where it belongs
}

static void AnimationMovesTheBoneInItsOwnSpace()
{
    Rig rig = MakeRig(true);
    AnimationClip clip("slide", 1.0f);
    clip.SetTicksPerSecond(1.0f);
    Keyframe a, b;
    a.TimeStamp = 0.0f; a.Translation = { 0, 1, 0 };                // the bind position of the root bone
    b.TimeStamp = 1.0f; b.Translation = { 2, 1, 0 };                // slid 2 along its own X
    clip.AddBoneAnimation("root", { a, b });

    AnimationEvaluator evaluator;
    std::vector<XMFLOAT4X4> out;
    evaluator.Evaluate(clip, rig.skeleton, 1.0f, out);

    // the slide is in the space ABOVE the bone (before the ancestors): the vertex moves by (2,0,0) and is then placed like
    // a static mesh: (v + (2,0,0)) * ancestors
    const XMVECTOR v = XMVectorSet(0.3f, -0.2f, 0.7f, 1.0f);
    const XMVECTOR expected = XMVector3Transform(XMVectorAdd(v, XMVectorSet(2, 0, 0, 0)), rig.ancestors);
    CHECK(NearV(XMVector3Transform(v, RowForm(out[0])), expected));
}

static void RootBonesWithoutAncestorsAreUnchanged()
{
    // a skeleton whose root bone has nothing above it (identity RootParentTransform): exactly what it was before
    Skeleton sk;
    Bone root; root.Name = "root"; root.LocalTransform = Store(XMMatrixTranslation(0, 2, 0));
    root.OffsetMatrix = Store(XMMatrixTranslation(0, -2, 0));
    sk.AddBone(root);
    AnimationClip bind("bind", 1.0f);
    AnimationEvaluator evaluator;
    std::vector<XMFLOAT4X4> out;
    evaluator.Evaluate(bind, sk, 0.0f, out);
    const XMMATRIX m = RowForm(out[0]);
    CHECK(NearV(XMVector3Transform(XMVectorSet(1, 2, 3, 1), m), XMVectorSet(1, 2, 3, 1)));      // offset * world = identity
}

// ---- the box of a skinned mesh
static std::vector<XMFLOAT4X4> GpuPose(std::initializer_list<XMMATRIX> rowForm)
{
    std::vector<XMFLOAT4X4> pose;
    for (const XMMATRIX& m : rowForm) pose.push_back(Store(XMMatrixTranspose(m)));     // the GPU form
    return pose;
}

static void SkinnedBoundsFollowTheBones()
{
    // a unit cube's corners, all on bone 0, and a pose that scales by 10 and lifts by 5 (a node transform in the bind pose)
    std::vector<XMFLOAT3> pos;
    for (int i = 0; i < 8; ++i) pos.push_back({ (i & 1) ? 1.0f : 0.0f, (i & 2) ? 1.0f : 0.0f, (i & 4) ? 1.0f : 0.0f });
    std::vector<XMINT4> idx(8, XMINT4(0, 0, 0, 0));
    std::vector<XMFLOAT4> w(8, XMFLOAT4(1, 0, 0, 0));

    auto box = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixScaling(10, 10, 10) * XMMatrixTranslation(0, 5, 0) }) }, 0.0f);
    CHECK(box.valid() && Near(box.min.x, 0) && Near(box.max.x, 10) && Near(box.min.y, 5) && Near(box.max.y, 15) && Near(box.max.z, 10));

    // the raw positions would have said 0..1: that is the bug (a box ten times too small, five units too low)
    auto raw = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixIdentity() }) }, 0.0f);
    CHECK(Near(raw.max.x, 1) && Near(raw.max.y, 1));

    // several poses: the union (a bone that swings out to x = 20 in the second pose)
    box = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixIdentity() }), GpuPose({ XMMatrixTranslation(20, 0, 0) }) }, 0.0f);
    CHECK(Near(box.min.x, 0) && Near(box.max.x, 21));

    // padding grows the box by a fraction of its size
    box = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixScaling(10, 10, 10) }) }, 0.1f);
    CHECK(Near(box.min.x, -1) && Near(box.max.x, 11));
}

static void SkinnedBoundsBlendInfluences()
{
    // one vertex half on a bone that stays and half on a bone that moves 10 along x: it ends up 5 along x
    std::vector<XMFLOAT3> pos = { { 0, 0, 0 } };
    std::vector<XMINT4> idx = { XMINT4(0, 1, 0, 0) };
    std::vector<XMFLOAT4> w = { XMFLOAT4(0.5f, 0.5f, 0, 0) };
    auto box = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixIdentity(), XMMatrixTranslation(10, 0, 0) }) }, 0.0f);
    CHECK(Near(box.min.x, 5) && Near(box.max.x, 5));

    // weights that do not add up to 1 are normalised, not shrunk towards the origin
    w = { XMFLOAT4(0.25f, 0.25f, 0, 0) };
    box = SkinnedBounds::Compute(pos, idx, w, { GpuPose({ XMMatrixIdentity(), XMMatrixTranslation(10, 0, 0) }) }, 0.0f);
    CHECK(Near(box.min.x, 5));
}

static void SkinnedBoundsEdgeCases()
{
    std::vector<XMFLOAT3> pos = { { 1, 2, 3 } };
    std::vector<XMINT4> idx = { XMINT4(0, 0, 0, 0) };
    std::vector<XMFLOAT4> none = { XMFLOAT4(0, 0, 0, 0) };
    // no weights: not skinned, counts where it is
    auto box = SkinnedBounds::Compute(pos, idx, none, { GpuPose({ XMMatrixTranslation(100, 0, 0) }) }, 0.0f);
    CHECK(Near(box.min.x, 1) && Near(box.max.y, 2) && Near(box.max.z, 3));

    // a bone index the pose does not have is skipped, a negative one too (the vertex then counts where it is)
    std::vector<XMINT4> bad = { XMINT4(7, -1, 0, 0) };
    std::vector<XMFLOAT4> full = { XMFLOAT4(0.5f, 0.5f, 0, 0) };
    box = SkinnedBounds::Compute(pos, bad, full, { GpuPose({ XMMatrixTranslation(100, 0, 0) }) }, 0.0f);
    CHECK(Near(box.min.x, 1));

    // nothing to compute from: an invalid box, never a made-up one
    CHECK(!SkinnedBounds::Compute({}, {}, {}, { GpuPose({ XMMatrixIdentity() }) }).valid());
    CHECK(!SkinnedBounds::Compute(pos, idx, full, {}).valid());

    // a NaN in a pose does not poison the box
    std::vector<XMFLOAT4X4> broken = GpuPose({ XMMatrixIdentity() });
    broken[0]._11 = NAN;
    box = SkinnedBounds::Compute(pos, idx, full, { broken, GpuPose({ XMMatrixIdentity() }) }, 0.0f);
    CHECK(box.valid() && std::isfinite(box.min.x) && std::isfinite(box.max.x));
}

int main()
{
    SkinnedBoundsFollowTheBones();
    SkinnedBoundsBlendInfluences();
    SkinnedBoundsEdgeCases();
    BindPoseEqualsTheMeshNodesTransform();
    WithoutTheAncestorsTheMeshIsWrong();
    AnimationMovesTheBoneInItsOwnSpace();
    RootBonesWithoutAncestorsAreUnchanged();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return EXIT_FAILURE; }
    std::printf("skeleton_tests: all passed\n");
    return EXIT_SUCCESS;
}
