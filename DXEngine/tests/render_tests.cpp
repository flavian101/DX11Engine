// Renderer-side logic that needs no GPU: scene extraction (culling, routing, sorting, material resolution).
// (Light packing has its own tests: light_tests.cpp.) Uses the real Mesh / Material / Scene classes; meshes are created without a device.
#include "renderer/Core/SceneTarget.h"
#include "renderer/Systems/SceneExtraction.h"
#include "scene/Scene.h"
#include "scene/AnimationSystem.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/Mesh/Utils/TangentMath.h"
#include "utils/Mesh/Utils/VertexAttribute.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <set>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-3f) { return std::fabs(a - b) < e; }

// ------------------------------------------------------------ helpers
static ExtractionCamera MakeCamera(XMFLOAT3 eye, XMFLOAT3 target)
{
    ExtractionCamera c;
    XMStoreFloat4x4(&c.view, XMMatrixLookAtLH(XMLoadFloat3(&eye), XMLoadFloat3(&target), XMVectorSet(0, 1, 0, 0)));
    XMStoreFloat4x4(&c.projection, XMMatrixPerspectiveFovLH(XM_PIDIV4, 16.0f / 9.0f, 0.1f, 100.0f));
    c.position = eye;
    return c;
}

static std::shared_ptr<Mesh> MakeCube(float size = 2.0f)
{
    return std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", size)), nullptr);
}

static std::shared_ptr<Material> MakeMaterial(const char* name, bool transparent = false)
{
    auto m = std::make_shared<Material>(name, ShadingModel::Lit);
    if (transparent) m->SetBlendMode(RHI::BlendMode::AlphaBlend);
    return m;
}

static Entity AddMesh(Scene& scene, const char* name, XMFLOAT3 pos, std::shared_ptr<Mesh> mesh, std::shared_ptr<Material> mat, Entity parent = Entity())
{
    Entity e = scene.CreateEntity(name, parent);
    e.Get<TransformComponent>().position = pos;
    auto& r = e.Add<MeshRendererComponent>();
    r.mesh = std::move(mesh);
    r.materials = { std::move(mat) };
    return e;
}

struct Result { BatchQueue q; RenderStatistics stats; };
static Result Run(Scene& scene, const ExtractionCamera& cam, const SceneExtraction& ex = SceneExtraction())
{
    Result r;
    TransformSystem::Update(scene);
    ex.Extract(scene, cam, r.q, r.stats);
    return r;
}

// ------------------------------------------------------------ extraction
static void TestCulling()
{
    Scene scene;
    auto mesh = MakeCube(), mat = std::shared_ptr<Mesh>(), unused = mesh; (void)unused; (void)mat;
    auto material = MakeMaterial("lit");
    AddMesh(scene, "front",  { 0, 0, 0 },     mesh, material);      // camera at z=-10 looks at +Z: in view
    AddMesh(scene, "behind", { 0, 0, -50 },   mesh, material);      // behind the camera
    AddMesh(scene, "side",   { 500, 0, 0 },   mesh, material);      // far to the right
    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.size() == 1);
    CHECK(r.stats.objectsSubmitted == 3 && r.stats.objectsCulled == 2);
}

static void TestCullingUsesTheMeshSize()   // needs real bounds (they used to be radius 0 for procedural meshes)
{
    Scene scene;
    auto material = MakeMaterial("lit");
    // centre is just outside the right edge of the view at z=0, but a 40-wide cube still overlaps it
    const ExtractionCamera cam = MakeCamera({ 0, 0, -10 }, { 0, 0, 0 });
    AddMesh(scene, "big",   { 12, 0, 0 }, MakeCube(40.0f), material);
    AddMesh(scene, "small", { 12, 0, 0 }, MakeCube(0.5f),  material);
    auto r = Run(scene, cam);
    CHECK(r.q.opaque.size() == 1);                      // only the big one reaches into the view
}

static void TestScaleAndParentAffectCulling()
{
    Scene scene;
    auto material = MakeMaterial("lit");
    Entity parent = scene.CreateEntity("parent");
    parent.Get<TransformComponent>().position = { 500, 0, 0 };           // far out of view
    AddMesh(scene, "child", { 0, 0, 0 }, MakeCube(), material, parent);  // local origin, world x = 500
    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.empty() && r.stats.objectsCulled == 1);

    parent.Get<TransformComponent>().position = { 0, 0, 0 };             // bring the parent into view
    r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.size() == 1);
}

static void TestSkyRoutingAndFollowing()
{
    Scene scene;
    Entity sky = AddMesh(scene, "sky", { 9999, 0, 0 }, MakeCube(100.0f), MakeMaterial("sky"));
    sky.Add<SkyboxComponent>();
    const XMFLOAT3 eye{ 3, 4, 5 };
    auto r = Run(scene, MakeCamera(eye, { 3, 4, 100 }));
    CHECK(r.q.background.size() == 1 && r.q.opaque.empty());             // never culled, though "far away"
    CHECK(Near(r.q.background[0].world._41, 3) && Near(r.q.background[0].world._42, 4) && Near(r.q.background[0].world._43, 5));
}

static void TestTransparentAndOpaqueOrdering()
{
    Scene scene;
    auto mesh = MakeCube();
    auto glass = MakeMaterial("glass", true);
    auto a = MakeMaterial("a"), b = MakeMaterial("b");
    AddMesh(scene, "g_near", { 0, 0, 0 },  mesh, glass);
    AddMesh(scene, "g_far",  { 0, 0, 20 }, mesh, glass);
    AddMesh(scene, "g_mid",  { 0, 0, 10 }, mesh, glass);
    AddMesh(scene, "a1", { -2, 0, 5 }, mesh, a); AddMesh(scene, "b1", { 0, 0, 6 }, mesh, b);
    AddMesh(scene, "a2", { 2, 0, 7 }, mesh, a);  AddMesh(scene, "b2", { 0, 0, 8 }, mesh, b);
    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));

    CHECK(r.q.transparent.size() == 3);
    CHECK(r.q.transparent[0].distanceToCamera > r.q.transparent[1].distanceToCamera &&
          r.q.transparent[1].distanceToCamera > r.q.transparent[2].distanceToCamera);   // far to near

    CHECK(r.q.opaque.size() == 4);
    // opaque items that share a material are adjacent (fewer state changes)
    int changes = 0;
    for (size_t i = 1; i < r.q.opaque.size(); ++i) if (r.q.opaque[i].material != r.q.opaque[i - 1].material) ++changes;
    CHECK(changes == 1);
}

static void TestHiddenAndConfig()
{
    Scene scene;
    auto material = MakeMaterial("lit");
    Entity hidden = AddMesh(scene, "hidden", { 0, 0, 0 }, MakeCube(), material);
    hidden.Get<MeshRendererComponent>().visible = false;
    AddMesh(scene, "off_screen", { 500, 0, 0 }, MakeCube(), material);
    const ExtractionCamera cam = MakeCamera({ 0, 0, -10 }, { 0, 0, 0 });

    auto r = Run(scene, cam);
    CHECK(r.q.opaque.empty() && r.stats.objectsSubmitted == 1);          // hidden entity is not even counted

    SceneExtraction off; off.SetConfig({ false, 1.05f });                // culling disabled
    r = Run(scene, cam, off);
    CHECK(r.q.opaque.size() == 1 && r.stats.objectsCulled == 0);
}

static void TestMaterialResolution()
{
    Scene scene;
    auto mesh = MakeCube();
    auto meshMaterial = MakeMaterial("mesh"), overrideMaterial = MakeMaterial("override");
    mesh->SetMaterial(meshMaterial);

    Entity own = scene.CreateEntity("own");                               // no override -> the mesh's material
    own.Add<MeshRendererComponent>().mesh = mesh;
    AddMesh(scene, "over", { 0, 0, 2 }, mesh, overrideMaterial);          // override wins
    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.size() == 2);
    int withMesh = 0, withOverride = 0;
    for (auto& i : r.q.opaque) { if (i.material == meshMaterial) ++withMesh; if (i.material == overrideMaterial) ++withOverride; }
    CHECK(withMesh == 1 && withOverride == 1);
}

static void TestBucketComesFromTheMaterialsRenderState()
{
    Scene scene;
    auto mesh = MakeCube();
    const auto cam = MakeCamera({ 0, 0, -10 }, { 0, 0, 0 });

    auto opaque = MakeMaterial("opaque");
    auto glass  = std::make_shared<Material>("glass", ShadingModel::Lit);      // transparent = Lit + a blended state
    glass->SetRenderState(RenderState::Blended());
    auto hologram = std::make_shared<Material>("hologram", ShadingModel::Unlit);   // ANY shading model can be blended
    hologram->SetRenderState(RenderState::Blended());
    auto backdrop = std::make_shared<Material>("backdrop", ShadingModel::Unlit);   // an explicit queue, no SkyboxComponent needed
    backdrop->GetRenderState().queue = RenderQueue::Background;
    auto hud = std::make_shared<Material>("hud", ShadingModel::Unlit);          // the overlay queue is not part of the 3D scene
    hud->SetRenderState(RenderState::Overlay());

    AddMesh(scene, "a", { 0, 0, 0 },  mesh, opaque);
    AddMesh(scene, "b", { 1, 0, 0 },  mesh, glass);
    AddMesh(scene, "c", { -1, 0, 0 }, mesh, hologram);
    AddMesh(scene, "d", { 0, 1, 0 },  mesh, backdrop);
    AddMesh(scene, "e", { 0, -1, 0 }, mesh, hud);
    Entity sky = AddMesh(scene, "f", { 0, 0, 3 }, mesh, MakeMaterial("sky-lit"));
    sky.Add<SkyboxComponent>();                                                // the component still forces the background bucket

    auto r = Run(scene, cam);
    CHECK(r.q.opaque.size() == 1);                       // only "a"
    CHECK(r.q.transparent.size() == 2);                  // glass and the unlit hologram: blended = transparent, whatever the model
    CHECK(r.q.background.size() == 2);                   // the explicit-queue material and the SkyboxComponent entity
    CHECK(r.q.opaque.size() + r.q.transparent.size() + r.q.background.size() == 5);   // the UI-queue material is not a 3D draw

    // a material whose state CHANGES moves bucket on the next extraction: nothing was cached
    glass->SetBlendMode(RHI::BlendMode::Opaque);
    r = Run(scene, cam);
    CHECK(r.q.opaque.size() == 2 && r.q.transparent.size() == 1);
}

// A skinned mesh is drawn with the bones of the Animator ABOVE it. No Animator above it, or none published yet, means the
// mesh is drawn unskinned instead of reading nothing; the items point at the Animator's own vector (no copy per frame).
static void TestSkinnedBonesAreOptionalAndSafe()
{
    Scene scene;
    auto material = MakeMaterial("lit");

    Entity rig = scene.CreateEntity("rig");
    auto& animator = rig.Add<AnimatorComponent>();
    Entity body = AddMesh(scene, "skinned", { 0, 0, 0 }, MakeCube(), material);
    scene.SetParent(body, rig);
    body.Add<SkinnedMeshComponent>();
    Entity loose = AddMesh(scene, "no_animator", { 0, 0, 3 }, MakeCube(), material);
    loose.Add<SkinnedMeshComponent>();                                    // skinned, but nothing above it: must not crash
    Entity plain = AddMesh(scene, "plain", { 0, 0, 6 }, MakeCube(), material);
    scene.SetParent(plain, rig);                                          // under the Animator but not skinned

    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.size() == 3);
    for (auto& i : r.q.opaque) CHECK(!i.IsSkinned());                     // no bones published yet = static

    animator.boneMatrices.assign(4, TransformComponent::Identity());
    r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    int skinned = 0;
    for (auto& i : r.q.opaque)
        if (i.IsSkinned()) { ++skinned; CHECK(i.boneMatrices == &animator.boneMatrices); }
    CHECK(skinned == 1);                                                  // only the skinned mesh that has an Animator above it
}

static void TestExtractionReusesCapacity()
{
    Scene scene;
    auto mesh = MakeCube(); auto material = MakeMaterial("lit");
    for (int i = 0; i < 50; ++i) AddMesh(scene, "n", { float(i % 5), 0, float(i / 5) }, mesh, material);
    BatchQueue q; RenderStatistics st; SceneExtraction ex;
    TransformSystem::Update(scene);
    ex.Extract(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }), q, st);
    const size_t cap = q.opaque.capacity();
    st.Reset();
    ex.Extract(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }), q, st);
    CHECK(q.opaque.capacity() == cap && q.opaque.size() == 50);
}

// ------------------------------------------------------------ scene target (the editor's viewport texture)
static void TestSceneTargetSize()
{
    // a panel that is collapsed or not laid out yet asks for nothing: draw into the back buffer
    CHECK(!ResolveSceneTargetSize(0, 0, 16384).Enabled());
    CHECK(!ResolveSceneTargetSize(800, 0, 16384).Enabled());
    CHECK(!ResolveSceneTargetSize(0, 600, 16384).Enabled());
    CHECK(!ResolveSceneTargetSize(-5, 600, 16384).Enabled());
    CHECK(!ResolveSceneTargetSize(800, 600, 0).Enabled());            // a device that reports no limit: nothing can be created

    // normal sizes pass through unchanged
    const SceneTargetSize a = ResolveSceneTargetSize(1280, 720, 16384);
    CHECK(a.Enabled() && a.width == 1280 && a.height == 720);
    CHECK(ResolveSceneTargetSize(1, 1, 16384).Enabled());

    // each side is clamped on its own; an oversized request keeps working instead of failing to create
    const SceneTargetSize big = ResolveSceneTargetSize(40000, 720, 16384);
    CHECK(big.width == 16384 && big.height == 720);
    const SceneTargetSize tall = ResolveSceneTargetSize(100, 99999, 4096);
    CHECK(tall.width == 100 && tall.height == 4096);

    // equal requests give equal sizes (Renderer compares them to decide whether to recreate the texture)
    CHECK(ResolveSceneTargetSize(640, 480, 16384) == ResolveSceneTargetSize(640, 480, 16384));
    CHECK(ResolveSceneTargetSize(640, 480, 16384) != ResolveSceneTargetSize(641, 480, 16384));
}

// Import-time optimisation (meshoptimizer): triangles are reordered for the GPU's vertex cache and the vertices move into
// the order the triangles use them. The picture must not change: every triangle still has the same three vertex POSITIONS
// in the same winding, inside the same submesh, and the submesh ranges stay where they are.
using PosTri = std::array<std::array<float, 3>, 3>;

static std::multiset<PosTri> PositionTriangles(const MeshResource& res, size_t first, size_t count)
{
    std::multiset<PosTri> s;
    for (size_t i = first; i + 2 < first + count; i += 3)
    {
        PosTri t;
        for (size_t k = 0; k < 3; ++k)
        {
            const XMFLOAT3 p = res.GetVertexData()->GetAttribute<XMFLOAT3>(res.GetIndexData()->GetIndex(i + k), RHI::VertexAttributeType::Position);
            t[k] = { p.x, p.y, p.z };
        }
        s.insert(t);
    }
    return s;
}

// Normal maps need a tangent frame. The procedural primitives used to be built WITHOUT tangents, so a normal map on the
// ground plane (or any primitive) was silently skipped by the shader. Every primitive now carries a unit tangent with w = +-1,
// perpendicular to its normal.
static void CheckPrimitiveHasTangentFrame(const MeshResource& resource)
{
    const VertexData* vertexData = resource.GetVertexData();
    CHECK(vertexData != nullptr);
    if (!vertexData) return;
    CHECK(vertexData->GetLayout().HasAttribute(RHI::VertexAttributeType::Tangent));

    for (size_t i = 0; i < vertexData->GetVertexCount(); ++i)
    {
        const DirectX::XMFLOAT3 n = vertexData->GetAttribute<DirectX::XMFLOAT3>(i, RHI::VertexAttributeType::Normal);
        const DirectX::XMFLOAT4 t = vertexData->GetAttribute<DirectX::XMFLOAT4>(i, RHI::VertexAttributeType::Tangent);
        const float length = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        const float dotNT = n.x * t.x + n.y * t.y + n.z * t.z;
        CHECK(std::fabs(length - 1.0f) < 1e-3f);
        CHECK(std::fabs(dotNT) < 1e-3f);
        CHECK(t.w == 1.0f || t.w == -1.0f);
    }
}

static void TestPrimitivesHaveTangents()
{
    CheckPrimitiveHasTangentFrame(*MeshResource::CreatePlane("plane", 1.0f, 1.0f, 1, 1));
    CheckPrimitiveHasTangentFrame(*MeshResource::CreateCube("cube", 1.0f));
    CheckPrimitiveHasTangentFrame(*MeshResource::CreateSphere("sphere", 1.0f, 16));
    CheckPrimitiveHasTangentFrame(*MeshResource::CreateCylinder("cylinder", 1.0f, 2.0f, 16));
    CheckPrimitiveHasTangentFrame(*MeshResource::CreateQuad("quad", 1.0f, 1.0f));

    // the ground plane lies in XZ with u along +X: its tangent is +X and a flat normal-map texel maps to +Y (up)
    auto plane = MeshResource::CreatePlane("plane", 1.0f, 1.0f, 1, 1);
    const DirectX::XMFLOAT4 t = plane->GetVertexData()->GetAttribute<DirectX::XMFLOAT4>(0, RHI::VertexAttributeType::Tangent);
    CHECK(std::fabs(t.x - 1.0f) < 1e-3f && std::fabs(t.y) < 1e-3f && std::fabs(t.z) < 1e-3f);
}

static void TestOptimizeKeepsTrianglesAndSubmeshes()
{
    auto resource = std::shared_ptr<MeshResource>(MeshResource::CreateSphere("sphere", 1.0f, 24));
    const size_t total = resource->GetIndexData()->GetIndexCount();
    CHECK(total > 600 && total % 3 == 0);

    // two submeshes at a triangle boundary: each must keep its own triangles
    const uint32_t split = static_cast<uint32_t>((total / 6) * 3);
    resource->AddSubMesh("first", 0, split, 0);
    resource->AddSubMesh("second", split, static_cast<uint32_t>(total) - split, 1);
    const auto firstBefore  = PositionTriangles(*resource, 0, split);
    const auto secondBefore = PositionTriangles(*resource, split, total - split);
    const size_t verticesBefore = resource->GetVertexData()->GetVertexCount();

    Mesh mesh(resource, nullptr);
    mesh.OptimizeForRendering();                          // (true or false: a sphere may already be well ordered)

    const MeshResource& res = *resource;
    CHECK(res.GetIndexData()->GetIndexCount() == total);
    CHECK(res.GetVertexData()->GetVertexCount() <= verticesBefore);
    CHECK(PositionTriangles(res, 0, split) == firstBefore);
    CHECK(PositionTriangles(res, split, total - split) == secondBefore);
    CHECK(res.GetSubMesh(0).indexStart == 0 && res.GetSubMesh(0).indexCount == split);
    CHECK(res.GetSubMesh(1).indexStart == split);

    // the submeshes' vertex ranges follow the new vertex order and stay inside the vertex buffer
    for (uint32_t s = 0; s < 2; ++s)
    {
        const SubMesh& sm = res.GetSubMesh(s);
        CHECK(sm.vertexCount > 0 && size_t(sm.vertexStart) + sm.vertexCount <= res.GetVertexData()->GetVertexCount());
    }

    // running it again is safe (the cache optimiser does not promise the same order twice, but never a different picture)
    mesh.OptimizeForRendering();
    CHECK(res.GetIndexData()->GetIndexCount() == total);
    CHECK(PositionTriangles(res, 0, split) == firstBefore);
    CHECK(PositionTriangles(res, split, total - split) == secondBefore);
}

// ------------------------------------------------------------ tangent frames (normal maps)
// Two triangles of a quad in the XY plane facing +Z; 'flipU' mirrors the UVs of the second triangle (a mirrored UV island).
static std::shared_ptr<MeshResource> TangentQuad(bool collapseSecond, bool mirrorSecond)
{
    auto vertices = std::make_unique<VertexData>(VertexLayout::CreateLit());
    vertices->Resize(6);
    const XMFLOAT3 pos[6] = { {0,0,0}, {1,0,0}, {0,1,0},   {2,0,0}, {3,0,0}, {2,1,0} };
    // image-style UVs (v grows DOWNWARD, as the importer flips them): u right, v = 1 - y
    XMFLOAT2 uv[6] = { {0,1}, {1,1}, {0,0},   {0,1}, {1,1}, {0,0} };
    if (mirrorSecond) { uv[3].x = 1; uv[4].x = 0; uv[5].x = 1; }
    if (collapseSecond) { uv[3] = uv[4] = uv[5] = { 0.5f, 0.5f }; }
    for (size_t i = 0; i < 6; ++i)
    {
        vertices->SetAttribute(i, RHI::VertexAttributeType::Position, pos[i]);
        vertices->SetAttribute(i, RHI::VertexAttributeType::Normal, XMFLOAT3{ 0, 0, -1 });   // LH: faces the camera at -Z
        vertices->SetAttribute(i, RHI::VertexAttributeType::TexCoord0, uv[i]);
    }
    auto indices = std::make_unique<IndexData>(IndexType::UInt16);
    indices->AddTriangle(0, 1, 2);
    indices->AddTriangle(3, 4, 5);
    auto resource = std::make_shared<MeshResource>("quad");
    resource->SetVertexData(std::move(vertices));
    resource->SetIndexData(std::move(indices));
    resource->GenerateTangents();
    return resource;
}

static XMFLOAT3 BitangentOf(const MeshResource& r, size_t vertex)
{
    const XMFLOAT3 n = r.GetVertexData()->GetAttribute<XMFLOAT3>(vertex, RHI::VertexAttributeType::Normal);
    const XMFLOAT4 t = r.GetVertexData()->GetAttribute<XMFLOAT4>(vertex, RHI::VertexAttributeType::Tangent);
    // what the shader builds: B = cross(N, T) * w
    return { (n.y * t.z - n.z * t.y) * t.w, (n.z * t.x - n.x * t.z) * t.w, (n.x * t.y - n.y * t.x) * t.w };
}

static void TestGeneratedTangentsFollowTheUvs()
{
    auto r = TangentQuad(false, false);
    const XMFLOAT4 t0 = r->GetVertexData()->GetAttribute<XMFLOAT4>(0, RHI::VertexAttributeType::Tangent);
    CHECK(Near(t0.x, 1) && Near(t0.y, 0) && Near(t0.z, 0));               // T along +u (here +X)
    const XMFLOAT3 b0 = BitangentOf(*r, 0);
    CHECK(Near(b0.y, 1) && Near(b0.x, 0) && Near(b0.z, 0));               // B = image UP (+Y): where v DEcreases (OpenGL normal maps)
    CHECK(Near(BitangentOf(*r, 3).y, 1));                                 // the same on the second triangle
}

static void TestMirroredUvIslandsGetTheOppositeHandedness()
{
    auto r = TangentQuad(false, true);                                    // the second triangle is mirrored in u
    const XMFLOAT4 a = r->GetVertexData()->GetAttribute<XMFLOAT4>(0, RHI::VertexAttributeType::Tangent);
    const XMFLOAT4 b = r->GetVertexData()->GetAttribute<XMFLOAT4>(3, RHI::VertexAttributeType::Tangent);
    CHECK(a.w != b.w);                                                    // a constant w = 1 would light one of them upside down
    // in BOTH islands the bitangent still points image-up (+Y): that is what makes the map work on a mirrored island
    CHECK(Near(BitangentOf(*r, 0).y, 1) && Near(BitangentOf(*r, 3).y, 1));
    // and the tangent follows u in each: +X on the first, -X on the second (u runs the other way there)
    CHECK(a.x > 0.9f && b.x < -0.9f);
}

static void TestDegenerateUvsDoNotPoisonTheMesh()
{
    auto r = TangentQuad(true, false);                                    // the second triangle has collapsed UVs
    for (size_t i = 0; i < 6; ++i)
    {
        const XMFLOAT4 t = r->GetVertexData()->GetAttribute<XMFLOAT4>(i, RHI::VertexAttributeType::Tangent);
        CHECK(std::isfinite(t.x) && std::isfinite(t.y) && std::isfinite(t.z) && std::isfinite(t.w));
        CHECK(Near(t.x * t.x + t.y * t.y + t.z * t.z, 1.0f, 1e-3f));      // a usable unit tangent (a fallback frame), never NaN
    }
    const XMFLOAT4 fallback = r->GetVertexData()->GetAttribute<XMFLOAT4>(3, RHI::VertexAttributeType::Tangent);
    CHECK(std::fabs(fallback.z) < 1e-3f);                                 // perpendicular to the normal (-Z): lies in the XY plane
}

static void TestHandednessHelper()
{
    const XMFLOAT3 n = { 0, 0, 1 }, t = { 1, 0, 0 };
    CHECK(TangentMath::Handedness(n, t, { 0, 1, 0 }) == 1.0f);            // cross(N, T) = +Y
    CHECK(TangentMath::Handedness(n, t, { 0, -1, 0 }) == -1.0f);
    CHECK(TangentMath::Handedness(n, t, { 0, 0, 0 }) == 1.0f);            // no bitangent: the default
    const XMFLOAT3 p = TangentMath::AnyPerpendicular({ 0, 1, 0 });
    CHECK(Near(p.x * p.x + p.y * p.y + p.z * p.z, 1.0f) && Near(p.y, 0.0f));
}

// ------------------------------------------------------------ skinning vertex data
// The importer writes the four bone indices as an XMINT4 (16 bytes) and then the four weights. If the layout reserves less room
// for the indices than that, the weights overwrite indices 1-3: a vertex keeps one real bone and its other influences land on
// bone 0 (joints, neck, jaw and legs come out skewed). The layout must hold what is written, and every vertex must read back intact.
static void TestSkinningDataSurvivesTheLayout()
{
    auto vertices = std::make_unique<VertexData>(VertexLayout::CreateSkinned());
    const VertexLayout& layout = vertices->GetLayout();
    const RHI::VertexAttribute* idx = layout.FindAttribute(RHI::VertexAttributeType::BlendIndices, 0);
    const RHI::VertexAttribute* wgt = layout.FindAttribute(RHI::VertexAttributeType::BlendWeights, 0);
    CHECK(idx && wgt);
    if (!idx || !wgt) return;
    CHECK(idx->GetSize() >= sizeof(XMINT4));                              // room for four 32-bit indices
    CHECK(wgt->GetSize() >= sizeof(XMFLOAT4));
    CHECK(wgt->Offset >= idx->Offset + sizeof(XMINT4) || idx->Offset >= wgt->Offset + sizeof(XMFLOAT4));   // the two do not overlap

    vertices->Resize(3);
    const XMINT4   bones[3]   = { XMINT4(3, 17, 41, 70), XMINT4(1, 2, 3, 4), XMINT4(70, 0, 0, 0) };
    const XMFLOAT4 weights[3] = { XMFLOAT4(0.4f, 0.3f, 0.2f, 0.1f), XMFLOAT4(0.25f, 0.25f, 0.25f, 0.25f), XMFLOAT4(1, 0, 0, 0) };
    for (size_t v = 0; v < 3; ++v)                                        // written the way MeshProcessor does: indices, then weights
    {
        vertices->SetAttribute(v, RHI::VertexAttributeType::BlendIndices, bones[v]);
        vertices->SetAttribute(v, RHI::VertexAttributeType::BlendWeights, weights[v]);
    }
    for (size_t v = 0; v < 3; ++v)                                        // read back after ALL are written: a neighbour must not clobber it
    {
        const XMINT4   b = vertices->GetAttribute<XMINT4>(v, RHI::VertexAttributeType::BlendIndices);
        const XMFLOAT4 w = vertices->GetAttribute<XMFLOAT4>(v, RHI::VertexAttributeType::BlendWeights);
        CHECK(b.x == bones[v].x && b.y == bones[v].y && b.z == bones[v].z && b.w == bones[v].w);
        CHECK(Near(w.x, weights[v].x) && Near(w.y, weights[v].y) && Near(w.z, weights[v].z) && Near(w.w, weights[v].w));
    }
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "extraction: frustum culling",                       TestCulling },
        { "extraction: culling uses the mesh's real size",     TestCullingUsesTheMeshSize },
        { "extraction: parent transform affects culling",      TestScaleAndParentAffectCulling },
        { "extraction: sky routing, follows camera, no cull",  TestSkyRoutingAndFollowing },
        { "extraction: transparent / opaque ordering",         TestTransparentAndOpaqueOrdering },
        { "extraction: hidden entities, culling switch",       TestHiddenAndConfig },
        { "extraction: material resolution",                   TestMaterialResolution },
        { "extraction: bucket follows the material's state",   TestBucketComesFromTheMaterialsRenderState },
        { "extraction: skinned bones optional and safe",       TestSkinnedBonesAreOptionalAndSafe },
        { "extraction: no per-frame reallocation",             TestExtractionReusesCapacity },
        { "scene target: size resolution",                     TestSceneTargetSize },
        { "mesh: primitives carry tangents (normal maps work)", TestPrimitivesHaveTangents },
        { "mesh: cache optimisation keeps triangles + submeshes", TestOptimizeKeepsTrianglesAndSubmeshes },
        { "tangents: generated ones follow the UVs (GL style)",   TestGeneratedTangentsFollowTheUvs },
        { "tangents: a mirrored UV island gets the other handedness", TestMirroredUvIslandsGetTheOppositeHandedness },
        { "tangents: collapsed UVs give a finite frame",           TestDegenerateUvsDoNotPoisonTheMesh },
        { "tangents: handedness helper",                           TestHandednessHelper },
        { "skinning: bone indices + weights survive the layout",   TestSkinningDataSurvivesTheLayout },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll render tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
