// Editor UI smoke test: runs the real panels through real ImGui frames with NO window and NO GPU.
// ImGui's own assertions are the oracle: unbalanced Begin/End, Push/Pop, BeginDisabled/EndDisabled, duplicate ids in a
// list, a drag-drop source without a target... all abort. So "no abort" means the panels are structurally sound and
// every code path that draws a widget was executed. (What it cannot say: whether it LOOKS right. That needs eyes.)
//   build: see DXEngine/tests/CMakeLists.txt  (links the ImGui core files, not the backends)
#include "editor/EditorContext.h"
#include "editor/HierarchyPanel.h"
#include "editor/InspectorPanel.h"
#include "editor/ViewportPanel.h"
#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "Core/Input.h"
#include "renderer/Renderer.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include "imgui.h"
#include <cstdio>

using namespace DXEngine;

// The renderer is not linked into this test: the two functions the viewport panel calls.
namespace DXEngine
{
    static int g_lastSceneW = -1, g_lastSceneH = -1;
    void Renderer::SetSceneTargetSize(int w, int h) { g_lastSceneW = w; g_lastSceneH = h; }
    RHI::ITexture* Renderer::GetSceneTexture() { return nullptr; }
}

// The camera behaviours poll Input; there is no window here, so nothing is ever pressed.
namespace DXEngine
{
    Input* Input::s_Instance = nullptr;
    class NoInput : public Input
    {
    public:
        bool IsKeyPressedImpl(int) override { return false; }
        bool IsButtonPressedImpl(int) override { return false; }
        std::pair<float, float> GetMousePositionImpl() override { return { 0.0f, 0.0f }; }
        float GetMouseXImpl() override { return 0.0f; }
        float GetMouseYImpl() override { return 0.0f; }
    };
}

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)

// ---- a backend with no GPU: it only has to answer ImGui's texture requests
static void ServiceTextures()
{
    for (ImTextureData* tex : ImGui::GetPlatformIO().Textures)
    {
        if (tex->Status == ImTextureStatus_WantCreate)
        {
            tex->SetTexID(static_cast<ImTextureID>(1));
            tex->SetStatus(ImTextureStatus_OK);
        }
        else if (tex->Status == ImTextureStatus_WantUpdates)
            tex->SetStatus(ImTextureStatus_OK);
        else if (tex->Status == ImTextureStatus_WantDestroy)
            tex->SetStatus(ImTextureStatus_Destroyed);
    }
}

template<typename Fn> static void Frame(Fn&& fn)
{
    ImGui::NewFrame();
    fn();
    ImGui::Render();
    ServiceTextures();
}

struct UiEnv
{
    UiEnv()
    {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1280, 720);
        io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable | ImGuiConfigFlags_NavEnableKeyboard;
        io.DeltaTime = 1.0f / 60.0f;
    }
    ~UiEnv() { ImGui::DestroyContext(); }
};

static std::shared_ptr<Mesh> MakeCube()
{
    return std::make_shared<Mesh>(std::shared_ptr<MeshResource>(MeshResource::CreateCube("cube", 2.0f)), nullptr);
}

// A scene with every kind of thing the panels draw.
static void Populate(Scene& scene, EditorContext& ctx)
{
    ctx.scene = &scene;
    Entity root = scene.CreateEntity("Root");
    Entity group = scene.CreateEntity("Group", root);
    Entity cube = scene.CreateEntity("Cube", group);
    MeshRendererComponent mr;
    mr.mesh = MakeCube();
    mr.materials.push_back(MaterialFactory::CreateLitMaterial("Stone"));
    mr.materials.push_back(MaterialFactory::CreateEmissiveMaterial("Glow"));
    mr.materials.push_back(MaterialFactory::CreateUnlitMaterial("Flat"));
    cube.Add<MeshRendererComponent>(mr);
    Entity sun = scene.CreateEntity("Sun");
    LightComponent sunLight; sunLight.type = LightType::Directional;
    sun.Add<LightComponent>(sunLight);
    Entity spot = scene.CreateEntity("Spot", root);
    LightComponent spotLight; spotLight.type = LightType::Spot; spotLight.castsShadows = true;
    spot.Add<LightComponent>(spotLight);
    Entity lamp = scene.CreateEntity("Lamp");
    lamp.Add<LightComponent>();
    scene.CreateEntity("Sky").Add<SkyboxComponent>();
    scene.CreateEntity("Skinned").Add<SkinnedMeshComponent>();
    for (int i = 0; i < 300; ++i) scene.CreateEntity("light " + std::to_string(i));   // enough rows to need the clipper
    TransformSystem::Update(scene);
}

static void TestHierarchyPanel()
{
    UiEnv env;
    Scene scene; EditorContext ctx; Populate(scene, ctx);
    HierarchyPanel panel;
    bool open = true;

    for (int i = 0; i < 4; ++i) Frame([&] { panel.OnImGui(ctx, &open); });      // plain frames

    // a selection made elsewhere (a viewport pick) must be revealed and scrolled to
    Entity target;
    for (auto [e, n] : scene.Registry().view<NameComponent>().each())
        if (n.name == "Cube") target = scene.Wrap(e);
    ctx.selection.Set(target);
    ctx.revealSelection = true;
    for (int i = 0; i < 3; ++i) Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(!ctx.revealSelection);                                                // consumed
    CHECK(ctx.selection.Is(target));

    // every kind of selection draws without trouble
    for (auto [e, n] : scene.Registry().view<NameComponent>().each())
    {
        ctx.selection.Set(scene.Wrap(e));
        Frame([&] { panel.OnImGui(ctx, &open); });
    }

    // closed / no scene
    open = false;
    Frame([&] { panel.OnImGui(ctx, &open); });
    open = true;
    ctx.scene = nullptr;
    Frame([&] { panel.OnImGui(ctx, &open); });
}

static void TestInspectorPanel()
{
    UiEnv env;
    Scene scene; EditorContext ctx; Populate(scene, ctx);
    InspectorPanel panel;
    bool open = true;

    Frame([&] { panel.OnImGui(ctx, &open); });                                  // nothing selected

    // every entity of the scene, each drawn for several frames (headers open, all widgets submitted)
    for (auto [e, n] : scene.Registry().view<NameComponent>().each())
    {
        ctx.selection.Set(scene.Wrap(e));
        for (int i = 0; i < 3; ++i) Frame([&] { panel.OnImGui(ctx, &open); });
    }

    // an entity destroyed while selected reads as "nothing selected"
    Entity victim = scene.CreateEntity("victim");
    ctx.selection.Set(victim);
    Frame([&] { panel.OnImGui(ctx, &open); });
    scene.DestroyEntity(victim);
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(!ctx.selection.Has());

    open = false;
    Frame([&] { panel.OnImGui(ctx, &open); });
}

static void TestViewportPanel()
{
    UiEnv env;
    Scene scene; EditorContext ctx; Populate(scene, ctx);
    ViewportPanel panel;
    bool open = true;

    // no camera, no texture (the first frames of a real run): must not crash, must still ask for a scene texture
    for (int i = 0; i < 3; ++i) Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(g_lastSceneW >= 1 && g_lastSceneH >= 1);
    CHECK(panel.TargetSize().width >= 1 && panel.TargetSize().height >= 1);
    CHECK(!ctx.viewportHovered);

    // with a selection (the outline path needs a camera: absent here, so it is skipped) and every tool
    ctx.selection.Set(scene.Wrap(scene.Registry().view<NameComponent>().begin()[0]));
    for (GizmoOperation op : { GizmoOperation::None, GizmoOperation::Translate, GizmoOperation::Rotate, GizmoOperation::Scale })
    {
        ctx.gizmoSettings.operation = op;
        Frame([&] { panel.OnImGui(ctx, &open); });
    }

    // a hidden panel keeps the last size instead of asking for nothing
    const int w = g_lastSceneW, h = g_lastSceneH;
    open = false;
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(g_lastSceneW == w && g_lastSceneH == h);
}

static float Distance(const DirectX::XMFLOAT3& a, const DirectX::XMFLOAT3& b)
{
    return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z));
}

// The viewport with a real camera: aspect follows the panel, the outline path runs, a click picks, F frames the selection.
static void TestViewportWithCamera()
{
    static NoInput noInput;
    Input::Init(&noInput);

    UiEnv env;
    Scene scene; EditorContext ctx; ctx.scene = &scene;
    ctx.camera = std::make_shared<CameraController>();
    ctx.camera->SetFollowWindowAspect(false);

    Entity cube = scene.CreateEntity("Cube");
    MeshRendererComponent mr; mr.mesh = MakeCube();
    cube.Add<MeshRendererComponent>(mr);
    Entity lamp = scene.CreateEntity("Lamp");
    lamp.Add<LightComponent>().type = LightType::Spot;
    lamp.Get<TransformComponent>().position = { 3, 2, 0 };
    TransformSystem::Update(scene);

    Camera& camera = *ctx.camera->GetCamera();
    camera.SetPosition({ 0, 0, 10 });
    camera.Update(0.0f);
    const DirectX::XMFLOAT3 forward = camera.GetForwardVector();
    CHECK(forward.z < -0.9f);                                  // the camera looks along -Z: the cube is dead ahead

    ViewportPanel panel;
    bool open = true;
    auto draw = [&]
    {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(800, 500));
        panel.OnImGui(ctx, &open);
    };
    ImGuiIO& io = ImGui::GetIO();

    for (int i = 0; i < 3; ++i) Frame(draw);
    CHECK(panel.TargetSize().width > 700 && panel.TargetSize().height > 400);   // the panel's content area, not the window's
    CHECK(g_lastSceneW == panel.TargetSize().width && g_lastSceneH == panel.TargetSize().height);

    // the outline of every kind of selection (box / marker / light aim), with a camera
    for (Entity e : { cube, lamp })
    {
        ctx.selection.Set(e);
        for (int i = 0; i < 2; ++i) Frame(draw);
    }

    // a click on the middle of the picture picks the cube (the camera looks straight at it) ...
    ctx.selection.Clear();
    io.AddMousePosEvent(500.0f, 350.0f);
    Frame(draw);
    CHECK(ctx.viewportHovered);                                // over the picture, not over a toolbar widget
    io.AddMouseButtonEvent(0, true);
    Frame(draw);
    io.AddMouseButtonEvent(0, false);
    Frame(draw);
    CHECK(ctx.selection.Is(cube));
    CHECK(ctx.revealSelection);                                // the hierarchy is told to show it

    // a camera looking away from the cube must not pick it (the ray points the other way)
    ctx.selection.Clear();
    camera.SetPosition({ 0, 0, -10 });
    camera.Update(0.0f);
    io.AddMousePosEvent(500.0f, 350.0f);
    Frame(draw);
    io.AddMouseButtonEvent(0, true);
    Frame(draw);
    io.AddMouseButtonEvent(0, false);
    Frame(draw);
    CHECK(!ctx.selection.Has());
    camera.SetPosition({ 0, 0, 10 });
    camera.Update(0.0f);

    // ... a click on empty space clears the selection
    ctx.revealSelection = false;
    io.AddMousePosEvent(850.0f, 150.0f);
    Frame(draw);
    io.AddMouseButtonEvent(0, true);
    Frame(draw);
    io.AddMouseButtonEvent(0, false);
    Frame(draw);
    CHECK(!ctx.selection.Has());

    // not while flying
    ctx.flying = true;
    io.AddMousePosEvent(500.0f, 350.0f);
    Frame(draw);
    io.AddMouseButtonEvent(0, true);
    Frame(draw);
    io.AddMouseButtonEvent(0, false);
    Frame(draw);
    CHECK(!ctx.selection.Has());
    ctx.flying = false;

    // F brings the selection into view: the camera moves along its view direction until the cube is close
    ctx.selection.Set(cube);
    camera.SetPosition({ 0, 0, 200 });
    const DirectX::XMFLOAT3 cubeCenter = cube.Get<TransformComponent>().WorldPosition();
    const float before = Distance(camera.GetPositionFloat3(), cubeCenter);
    io.AddKeyEvent(ImGuiKey_F, true);
    Frame(draw);
    io.AddKeyEvent(ImGuiKey_F, false);
    Frame(draw);
    const float after = Distance(camera.GetPositionFloat3(), cubeCenter);
    CHECK(after < before * 0.2f && after > 1.0f);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "hierarchy panel: rows, reveal, every selection", TestHierarchyPanel },
        { "inspector panel: every entity kind",             TestInspectorPanel },
        { "viewport panel: size request, tools, hidden",    TestViewportPanel },
        { "viewport panel: camera, outline, pick, frame",   TestViewportWithCamera },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll editor UI tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
