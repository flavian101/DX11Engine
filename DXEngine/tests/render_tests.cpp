// Renderer-side logic that needs no GPU: scene extraction (culling, routing, sorting, material resolution).
// (Light packing has its own tests: light_tests.cpp.) Uses the real Mesh / Material / Scene classes; meshes are created without a device.
#include "renderer/Systems/SceneExtraction.h"
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/material/Material.h"
#include <cmath>
#include <cstdio>

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
    auto m = std::make_shared<Material>(name, transparent ? MaterialType::Transparent : MaterialType::Lit);
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

static void TestSkinnedBonesAreOptionalAndSafe()
{
    Scene scene;
    auto material = MakeMaterial("lit");
    std::vector<XMFLOAT4X4> bones(4, TransformComponent::Identity());
    bool haveBones = false;

    Entity e = AddMesh(scene, "skinned", { 0, 0, 0 }, MakeCube(), material);
    e.Add<SkinnedMeshComponent>().boneMatrices = [&]() -> const std::vector<XMFLOAT4X4>* { return haveBones ? &bones : nullptr; };
    Entity empty = AddMesh(scene, "no_callback", { 0, 0, 3 }, MakeCube(), material);
    empty.Add<SkinnedMeshComponent>();                                    // component with NO callback: must not crash

    auto r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    CHECK(r.q.opaque.size() == 2 && !r.q.opaque[0].IsSkinned() && !r.q.opaque[1].IsSkinned());   // not available yet = static

    haveBones = true;
    r = Run(scene, MakeCamera({ 0, 0, -10 }, { 0, 0, 0 }));
    int skinned = 0; for (auto& i : r.q.opaque) if (i.IsSkinned()) ++skinned;
    CHECK(skinned == 1);
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
        { "extraction: skinned bones optional and safe",       TestSkinnedBonesAreOptionalAndSafe },
        { "extraction: no per-frame reallocation",             TestExtractionReusesCapacity },
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
