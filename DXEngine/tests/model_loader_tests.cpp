// ============================================================================
// model_loader_tests: the importer on the REAL sample models (assimp, textures, GPU meshes), then Instantiate and the
// Animator on what it produced. Needs a D3D11 device (the loader makes GPU meshes and textures): without one it prints
// SKIPPED and passes. Not part of the GPU-free suites: `ctest -L gpu`.
// ============================================================================
#include "RHI/GraphicsDevice.h"
#include "models/ModelAsset.h"
#include "models/ModelLoader.h"
#include "renderer/Renderer.h"
#include "scene/AnimationSystem.h"
#include "scene/Scene.h"
#include "scene/SceneFactory.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <windows.h>

using namespace DXEngine;
using namespace DirectX;

#ifndef DX_ASSETS_DIR
#define DX_ASSETS_DIR "Editor/assets"
#endif

// The loaders' default constructors ask the Renderer for its device; the test hands the loader a device itself, and this is
// the one function of Renderer.cpp those constructors link against.
static std::shared_ptr<RHI::IGraphicsDevice> g_device;
namespace DXEngine { std::shared_ptr<RHI::IGraphicsDevice> Renderer::GetDevice() { return g_device; } }

static int g_failures = 0;
#define CHECK(c) do { if (!(c)) { std::printf("FAIL line %d: %s\n", __LINE__, #c); ++g_failures; } } while (0)

static bool Finite(const XMFLOAT4X4& m)
{
    const float* f = &m._11;
    for (int i = 0; i < 16; ++i) if (!std::isfinite(f[i])) return false;
    return true;
}

static void CountEntities(Scene& scene, Entity e, size_t& entities, size_t& renderers, size_t& skinned)
{
    ++entities;
    if (e.Has<MeshRendererComponent>()) ++renderers;
    if (e.Has<SkinnedMeshComponent>()) ++skinned;
    for (Entity c : scene.GetChildren(e)) CountEntities(scene, c, entities, renderers, skinned);
}

// The size of an instance in the world (metres), from the meshes' boxes and the entities' world matrices.
static XMFLOAT3 WorldExtent(Scene& scene, Entity e, XMVECTOR& lo, XMVECTOR& hi)
{
    if (const MeshRendererComponent* r = e.TryGet<MeshRendererComponent>())
        if (r->mesh && r->mesh->GetResource())
        {
            const BoundingBox& b = r->mesh->GetResource()->GetBoundingBox();
            const XMMATRIX w = XMLoadFloat4x4(&e.Get<TransformComponent>().world);
            for (int i = 0; i < 8; ++i)
            {
                const XMVECTOR corner = XMVectorSet((i & 1) ? b.max.x : b.min.x, (i & 2) ? b.max.y : b.min.y, (i & 4) ? b.max.z : b.min.z, 1.0f);
                const XMVECTOR p = XMVector3Transform(corner, w);
                lo = XMVectorMin(lo, p); hi = XMVectorMax(hi, p);
            }
        }
    for (Entity c : scene.GetChildren(e)) WorldExtent(scene, c, lo, hi);
    XMFLOAT3 size; XMStoreFloat3(&size, XMVectorSubtract(hi, lo));
    return size;
}

static void CheckModel(ModelLoader& loader, const char* relativePath, bool expectAnimation)
{
    const std::string path = std::string(DX_ASSETS_DIR) + "/models/" + relativePath;
    if (!std::filesystem::exists(path)) { std::printf("  %-34s not found, skipped\n", relativePath); return; }

    auto asset = loader.LoadModel(path);
    CHECK(asset != nullptr);
    if (!asset) { std::printf("  %-34s FAILED to load: %s\n", relativePath, loader.GetLastError().c_str()); return; }

    const auto problems = asset->Validate();
    for (const std::string& p : problems) std::printf("    problem: %s\n", p.c_str());
    CHECK(problems.empty());
    CHECK(!asset->nodes.empty() && !asset->meshes.empty());
    CHECK(asset->GetTotalTriangleCount() > 0);
    CHECK(asset->name == std::filesystem::path(path).filename().string());

    // every mesh has a material for every submesh, and the asset lists the materials it uses
    for (const ModelMesh& m : asset->meshes)
    {
        CHECK(m.mesh && m.mesh->IsValid());
        const size_t submeshes = std::max<size_t>(1, m.mesh->GetSubmeshCount());
        for (size_t s = 0; s < submeshes; ++s) CHECK(m.mesh->GetMaterial(s) != nullptr);
    }
    CHECK(!asset->materials.empty());

    // the node tree: parents first, transforms finite
    for (size_t i = 0; i < asset->nodes.size(); ++i)
    {
        const ModelNode& n = asset->nodes[i];
        CHECK(std::isfinite(n.position.x) && std::isfinite(n.scale.x) && std::isfinite(n.rotation.w));
        CHECK(i == 0 || (n.parent >= 0 && size_t(n.parent) < i));
    }

    // instantiate
    Scene scene;
    Entity root = SceneFactory::Instantiate(scene, asset, "Instance");
    CHECK(root.IsValid());
    size_t entities = 0, renderers = 0, skinned = 0;
    CountEntities(scene, root, entities, renderers, skinned);
    CHECK(renderers >= 1);
    TransformSystem::Update(scene);
    CHECK(Finite(root.Get<TransformComponent>().world));

    size_t boneCount = 0, clipCount = 0;
    if (asset->IsSkinned())
    {
        CHECK(root.Has<AnimatorComponent>() && skinned >= 1);
        AnimatorComponent& a = root.Get<AnimatorComponent>();
        boneCount = a.skeleton->GetBoneCount();
        clipCount = a.clips.size();
        for (int i = 0; i < 5; ++i) AnimationSystem::Update(scene, 0.1f);
        if (!a.clips.empty())
        {
            CHECK(a.boneMatrices.size() == boneCount);
            for (const XMFLOAT4X4& m : a.boneMatrices) CHECK(Finite(m));
            CHECK(a.time > 0.0f && a.time <= a.clips[0]->GetDurationSeconds() + 1e-3f);
        }
    }
    if (expectAnimation) CHECK(asset->IsSkinned() && clipCount > 0);

    XMVECTOR lo = XMVectorReplicate(FLT_MAX), hi = XMVectorReplicate(-FLT_MAX);
    const XMFLOAT3 size = WorldExtent(scene, root, lo, hi);
    CHECK(std::isfinite(size.x) && size.x > 0 && size.y > 0 && size.z > 0);

    const XMFLOAT3 s = asset->nodes[0].scale;
    std::printf("  %-34s nodes %2zu  meshes %2zu  tris %7zu  materials %2zu  entities %3zu  bones %3zu  clips %2zu  rootScale %.3g  size %.2f x %.2f x %.2f m  warnings %zu\n",
        relativePath, asset->nodes.size(), asset->meshes.size(), asset->GetTotalTriangleCount(), asset->materials.size(),
        entities, boneCount, clipCount, s.x, size.x, size.y, size.z, asset->warnings.size());
    for (const std::string& w : asset->warnings) std::printf("      warning: %s\n", w.c_str());
}

int main()
{
    WNDCLASSA wc = {}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(nullptr); wc.lpszClassName = "model_loader_tests";
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("model_loader_tests", "model_loader_tests", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480, nullptr, nullptr, wc.hInstance, nullptr);
    ShowWindow(hwnd, SW_SHOW);

    std::shared_ptr<RHI::IGraphicsDevice> device(RHI::GraphicsDeviceFactory::Create(RHI::GraphicsAPI::DirectX11));
    if (!device || !device->Initialize(hwnd, 640, 480))
    {
        std::printf("model_loader_tests: SKIPPED (no D3D11 device could be created)\n");
        return 0;
    }
    g_device = device;

    ModelLoader loader(device);
    std::printf("models in %s/models:\n", DX_ASSETS_DIR);
    CheckModel(loader, "ring.gltf", false);
    CheckModel(loader, "cube/cube.fbx", false);
    CheckModel(loader, "lion/lionHead.fbx", false);
    CheckModel(loader, "ship/dutch_ship_large_02_1k.fbx", false);
    CheckModel(loader, "shark/scene.gltf", false);
    CheckModel(loader, "tunnel/future_tunnel.glb", false);

    // the cache: the same file with the same options is the same asset; ANY option that changes the result is a new one
    const std::string ring = std::string(DX_ASSETS_DIR) + "/models/ring.gltf";
    if (std::filesystem::exists(ring))
    {
        auto a = loader.LoadModel(ring);
        auto b = loader.LoadModel(ring);
        CHECK(a && a == b);
        ModelLoadOptions scaled; scaled.globalScale = 2.0f;
        auto c = loader.LoadModel(ring, scaled);
        CHECK(c && c != a);
        if (a && c) CHECK(std::fabs(c->nodes[0].scale.x - a->nodes[0].scale.x * 2.0f) < 1e-4f);   // the scale lives on the root node
        ModelLoadOptions noMaterials; noMaterials.loadMaterials = false;
        auto d = loader.LoadModel(ring, noMaterials);
        CHECK(d && d != a);
    }

    // a file that is not a model, and one that does not exist: no asset, an error message, no crash
    CHECK(loader.LoadModel(std::string(DX_ASSETS_DIR) + "/does_not_exist.fbx") == nullptr && loader.HasError());

    loader.ClearCache();
    g_device.reset();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return 1; }
    std::printf("model_loader_tests: all passed\n");
    return 0;
}
