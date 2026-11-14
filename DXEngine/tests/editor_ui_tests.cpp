// Editor UI smoke test: runs the real panels through real ImGui frames with NO window and NO GPU.
// ImGui's own assertions are the oracle: unbalanced Begin/End, Push/Pop, BeginDisabled/EndDisabled, duplicate ids in a
// list, a drag-drop source without a target... all abort. So "no abort" means the panels are structurally sound and
// every code path that draws a widget was executed. (What it cannot say: whether it LOOKS right. That needs eyes.)
//   build: see DXEngine/tests/CMakeLists.txt  (links the ImGui core files, not the backends)
#include "editor/EditorChrome.h"
#include "editor/EditorContext.h"
#include "editor/EditorIcons.h"
#include "editor/EditorTheme.h"
#include "editor/PropertyGrid.h"
#include "editor/RendererPanel.h"
#include "editor/ImGuizmoGizmo.h"
#include "editor/ViewportMath.h"
#include "editor/HierarchyPanel.h"
#include "editor/InspectorPanel.h"
#include "editor/ViewportPanel.h"
#include "camera/Camera.h"
#include "camera/CameraController.h"
#include "Core/Input.h"
#include "Core/UiInputBlocking.h"
#include "Event/MouseEvent.h"
#include "renderer/Renderer.h"
#include "renderer/Core/RenderTypes.h"
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

    // what the toolbar and the Renderer panel read and write: kept in plain variables
    static RenderEnvironment g_env;
    static bool g_wireframe = false, g_culling = true;
    static RenderStatistics g_stats;
    void Renderer::EnableWireframe(bool e) { g_wireframe = e; }
    bool Renderer::IsWireframeEnabled() { return g_wireframe; }
    void Renderer::EnableFrustumCulling(bool e) { g_culling = e; }
    bool Renderer::IsFrustumCullingEnabled() { return g_culling; }
    void Renderer::SetEnvironment(const RenderEnvironment& e) { g_env = e; }
    RenderEnvironment Renderer::GetEnvironment() { return g_env; }
    const RenderStatistics& Renderer::GetStats() { return g_stats; }
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

// Why the camera did not turn: ImGui keeps WantCaptureMouse true for as long as a button that went down over a window is
// held, wherever the pointer goes and even with ImGuiConfigFlags_NoMouse. This pins that behaviour down (if a newer ImGui
// changes it, the workaround in ImGuiLayer can go).
static void TestImGuiCapturesWhileAButtonIsHeld()
{
    UiEnv env;
    ImGuiIO& io = ImGui::GetIO();
    auto frame = [&] { Frame([] { ImGui::SetNextWindowPos(ImVec2(100, 100)); ImGui::SetNextWindowSize(ImVec2(400, 300)); ImGui::Begin("w"); ImGui::End(); }); };

    io.AddMousePosEvent(200, 200);
    frame(); frame();
    CHECK(io.WantCaptureMouse);                                 // over the window

    io.AddMousePosEvent(900, 650);
    frame();
    CHECK(!io.WantCaptureMouse);                                // outside, nothing held

    io.AddMousePosEvent(200, 200);
    frame();
    io.AddMouseButtonEvent(2, true);                            // the middle button goes down over the window ...
    frame();
    io.ConfigFlags |= ImGuiConfigFlags_NoMouse;                 // ... the camera takes the mouse ...
    io.AddMousePosEvent(900, 650);                              // ... and it is moved far away (re-centred every move)
    frame(); frame();
    CHECK(io.WantCaptureMouse);                                 // STILL true: the button is held

    UiInputState state;
    state.active = true;
    state.mouseEnabled = false;                                 // what ImGuiLayer tracks for a camera in use
    state.wantMouse = io.WantCaptureMouse;
    CHECK(!ShouldBlockMouseEvent(state));                       // so the events reach the camera anyway

    io.AddMouseButtonEvent(2, false);
    io.ConfigFlags &= ~ImGuiConfigFlags_NoMouse;
    frame(); frame();
    CHECK(!io.WantCaptureMouse);                                // released: back to normal
}

// The camera listens only when its gate says so, asked per event and per frame.
static void TestCameraInputGate()
{
    static NoInput noInput;
    Input::Init(&noInput);

    CameraController controller;
    bool listening = false;
    controller.SetInputGate([&] { return listening; });
    Camera& camera = *controller.GetCamera();
    camera.SetPosition({ 0, 0, 0 });
    camera.Update(0.0f);
    const DirectX::XMFLOAT3 start = camera.GetForwardVector();

    auto look = [&](float dx)
    {
        MouseMovedEvent e(dx, 0.0f);
        e.SetIsCaptured(true);
        controller.OnEvent(e);
        controller.Update(0.016f);
    };

    look(200.0f);
    DirectX::XMFLOAT3 now = camera.GetForwardVector();
    CHECK(Distance(start, now) < 1e-4f);                        // gate shut: the mouse does nothing

    listening = true;                                           // flips with no frame in between: the very next event counts
    look(200.0f);
    now = camera.GetForwardVector();
    CHECK(Distance(start, now) > 0.01f);                        // gate open: the view turns

    // and the deltas that arrived while it was shut were not saved up for later (that would be a jump)
    listening = false;
    look(500.0f);
    const DirectX::XMFLOAT3 held = camera.GetForwardVector();
    listening = true;
    controller.Update(0.016f);
    CHECK(Distance(held, camera.GetForwardVector()) < 1e-4f);
}

// ---- the gizmo, driven like a user would with the ENGINE's camera (left-handed, +Z forward in view space)
struct GizmoRig
{
    Camera camera;
    ImGuizmoGizmo gizmo;
    GizmoSettings settings;
    DirectX::XMFLOAT4X4 world;
    GizmoView view{};
    bool changedLastFrame = false;

    static constexpr float kX = 100, kY = 100, kW = 800, kH = 500;

    // 'diagonal': seen from a corner, so no axis points at the lens (ImGuizmo hides an axis that does)
    explicit GizmoRig(bool diagonal = false)
    {
        camera.SetProjectionParams(DirectX::XM_PIDIV4, kW / kH, 0.5f, 1000.0f);
        if (!diagonal)
        {
            camera.SetPosition({ 0.0f, 6.0f, 20.0f });
            camera.SetRotation({ -0.18f, 0.0f, 0.0f });
        }
        else
        {
            camera.SetPosition({ 14.0f, 12.0f, 14.0f });
            camera.SetRotation({ -0.5f, 0.78f, 0.0f });
            camera.Update(0.0f);
            const DirectX::XMFLOAT3 f = camera.GetForwardVector();            // yaw turns which way? make it face the origin
            if (f.x * 14.0f + f.z * 14.0f > 0.0f)
                camera.SetRotation({ -0.5f, -0.78f, 0.0f });
        }
        camera.Update(0.0f);
        DirectX::XMStoreFloat4x4(&world, DirectX::XMMatrixTranslation(0.0f, 1.4f, 0.0f));
        view.view = camera.GetViewMatrix4X4();
        view.projection = camera.GetProjection4X4();
        view.x = kX; view.y = kY; view.width = kW; view.height = kH;
    }

    void Frame1()
    {
        Frame([&]
        {
            ImGui::SetNextWindowPos(ImVec2(kX, kY));
            ImGui::SetNextWindowSize(ImVec2(kW, kH));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::Begin("vp", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground);
            ImGui::PopStyleVar();
            changedLastFrame = gizmo.Manipulate(view, settings, world);
            ImGui::End();
        });
    }

    // where a world point is on screen (absolute pixels)
    ImVec2 Screen(DirectX::XMFLOAT3 p) const
    {
        DirectX::XMFLOAT2 s;
        ViewportMath::ProjectToViewport(p, view.view, view.projection, kW, kH, s);
        return ImVec2(kX + s.x, kY + s.y);
    }

    DirectX::XMFLOAT3 Position() const { return { world._41, world._42, world._43 }; }

    // press on 'start', drag to 'end', release; a frame between every step so ImGui sees each state
    void Drag(ImVec2 start, ImVec2 end)
    {
        ImGuiIO& io = ImGui::GetIO();
        io.AddMousePosEvent(start.x, start.y);
        Frame1(); Frame1();
        io.AddMouseButtonEvent(0, true);
        Frame1();
        for (int i = 1; i <= 8; ++i)
        {
            io.AddMousePosEvent(start.x + (end.x - start.x) * i / 8.0f, start.y + (end.y - start.y) * i / 8.0f);
            Frame1();
        }
        io.AddMouseButtonEvent(0, false);
        Frame1(); Frame1();
    }
};

// Dragging a handle must move the entity along THAT axis, toward where the mouse went. This is also what proves the
// matrices of a left-handed D3D camera mean the same thing to ImGuizmo as they do to the engine.
static void TestGizmoTranslatesAlongTheDraggedAxis()
{
    UiEnv env;
    const DirectX::XMFLOAT3 axes[3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
    const char* names[3] = { "X", "Y", "Z" };

    for (int diagonal = 0; diagonal < 2; ++diagonal)
        for (int a = 0; a < 3; ++a)
        {
            if (!diagonal && a == 2) continue;                 // seen from the front the Z axis points at the lens: ImGuizmo hides it

            GizmoRig rig(diagonal != 0);
            rig.settings.operation = GizmoOperation::Translate;
            rig.settings.space = GizmoSpace::World;
            rig.Frame1();
            const DirectX::XMFLOAT3 start = rig.Position();

            // the handle's direction on screen: from the entity toward one unit along the axis
            const ImVec2 o = rig.Screen(start);
            const ImVec2 t = rig.Screen({ start.x + axes[a].x, start.y + axes[a].y, start.z + axes[a].z });
            ImVec2 d(t.x - o.x, t.y - o.y);
            const float len = std::sqrt(d.x * d.x + d.y * d.y);
            d.x /= len; d.y /= len;

            // press on the arm of the handle (30 px out), drag 60 px further along it
            rig.Drag(ImVec2(o.x + d.x * 30.0f, o.y + d.y * 30.0f), ImVec2(o.x + d.x * 90.0f, o.y + d.y * 90.0f));

            const DirectX::XMFLOAT3 end = rig.Position();
            const float moved[3] = { end.x - start.x, end.y - start.y, end.z - start.z };
            std::printf("    %s camera, axis %s: moved (%.2f, %.2f, %.2f)\n", diagonal ? "diagonal" : "front", names[a], moved[0], moved[1], moved[2]);
            const float along = moved[0] * axes[a].x + moved[1] * axes[a].y + moved[2] * axes[a].z;
            CHECK(along > 0.2f);                                                   // it moved, toward the drag
            for (int k = 0; k < 3; ++k)
                if (k != a) CHECK(std::fabs(moved[k]) < 0.25f * std::fabs(along));  // and (almost) nowhere else
        }
}

// The whole path a user takes: select an entity UNDER A SCALED, ROTATED PARENT, drag its gizmo handle in the viewport panel.
// The entity must land where the gizmo put it (in the world), and what is stored is its LOCAL transform.
static void TestViewportGizmoMovesAChildUnderATransformedParent()
{
    static NoInput noInput;
    Input::Init(&noInput);

    UiEnv env;
    Scene scene; EditorContext ctx; ctx.scene = &scene;
    ctx.gizmo = std::make_unique<ImGuizmoGizmo>();
    ctx.camera = std::make_shared<CameraController>();
    ctx.camera->SetFollowWindowAspect(false);

    Entity parent = scene.CreateEntity("parent");
    parent.Get<TransformComponent>().position = { 4, 0, -3 };
    parent.Get<TransformComponent>().scale = { 2, 2, 2 };
    parent.Get<TransformComponent>().SetEulerDegrees(0, 50, 0);
    Entity child = scene.CreateEntity("child", parent);
    child.Get<TransformComponent>().position = { 0.5f, 0.7f, 0.0f };
    TransformSystem::Update(scene);
    ctx.selection.Set(child);
    ctx.gizmoSettings.operation = GizmoOperation::Translate;
    ctx.gizmoSettings.space = GizmoSpace::World;

    Camera& camera = *ctx.camera->GetCamera();
    camera.SetPosition({ 14.0f, 12.0f, 14.0f });
    camera.SetRotation({ -0.5f, 0.78f, 0.0f });
    camera.Update(0.0f);
    const DirectX::XMFLOAT3 f = camera.GetForwardVector();
    if (f.x * 14.0f + f.z * 14.0f > 0.0f) camera.SetRotation({ -0.5f, -0.78f, 0.0f });
    camera.Update(0.0f);

    ViewportPanel panel;
    bool open = true;
    ImGuiIO& io = ImGui::GetIO();
    auto draw = [&]
    {
        ImGui::SetNextWindowPos(ImVec2(100, 100));
        ImGui::SetNextWindowSize(ImVec2(800, 500));
        panel.OnImGui(ctx, &open);
        TransformSystem::Update(scene);                     // what the application does every frame
    };
    for (int i = 0; i < 3; ++i) Frame(draw);                // the panel sizes the camera's aspect to its picture

    const ViewportPanel::Rect img = panel.ImageRect();
    CHECK(img.width > 700 && img.height > 400);
    DirectX::XMFLOAT4X4 view = camera.GetViewMatrix4X4(), proj = camera.GetProjection4X4();
    auto screen = [&](DirectX::XMFLOAT3 p)
    {
        DirectX::XMFLOAT2 s;
        ViewportMath::ProjectToViewport(p, view, proj, img.width, img.height, s);
        return ImVec2(img.x + s.x, img.y + s.y);
    };

    const DirectX::XMFLOAT3 before = child.Get<TransformComponent>().WorldPosition();
    const ImVec2 o = screen(before);
    const ImVec2 t = screen({ before.x + 1.0f, before.y, before.z });        // the WORLD x axis
    ImVec2 d(t.x - o.x, t.y - o.y);
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    d.x /= len; d.y /= len;

    io.AddMousePosEvent(o.x + d.x * 30.0f, o.y + d.y * 30.0f);
    Frame(draw); Frame(draw);
    io.AddMouseButtonEvent(0, true);
    Frame(draw);
    for (int i = 1; i <= 8; ++i)
    {
        io.AddMousePosEvent(o.x + d.x * (30.0f + 7.5f * i), o.y + d.y * (30.0f + 7.5f * i));
        Frame(draw);
    }
    io.AddMouseButtonEvent(0, false);
    Frame(draw); Frame(draw);

    const DirectX::XMFLOAT3 after = child.Get<TransformComponent>().WorldPosition();
    std::printf("    child world position moved (%.2f, %.2f, %.2f)\n", after.x - before.x, after.y - before.y, after.z - before.z);
    CHECK(after.x - before.x > 0.2f);                                          // along the dragged WORLD axis ...
    CHECK(std::fabs(after.y - before.y) < 0.05f && std::fabs(after.z - before.z) < 0.05f);   // ... and only along it, though the parent is rotated and scaled
    CHECK(ctx.selection.Is(child));                                            // grabbing the handle did not pick something else
    CHECK(!ctx.flying);
}

// ------------------------------------------------------------ the look
static void TestThemeScaling()
{
    UiEnv env;
    EditorTheme::Apply(1.0f);
    const float base = ImGui::GetStyle().FramePadding.y;
    CHECK(base > 0.0f && EditorTheme::Scale() == 1.0f);

    EditorTheme::Apply(1.5f);
    const float scaled = ImGui::GetStyle().FramePadding.y;
    CHECK(std::fabs(scaled - base * 1.5f) < 0.6f);                           // sizes follow the scale (ImGui rounds some of them)
    CHECK(std::fabs(ImGui::GetStyle().FontScaleMain - 1.5f) < 1e-4f);        // and so do the fonts

    EditorTheme::Apply(1.5f);                                                // applying again must not compound
    CHECK(std::fabs(ImGui::GetStyle().FramePadding.y - scaled) < 1e-4f);

    EditorTheme::Apply(1.0f);                                                // and going back is exact
    CHECK(std::fabs(ImGui::GetStyle().FramePadding.y - base) < 1e-4f);

    EditorTheme::Apply(0.1f);
    CHECK(EditorTheme::Scale() == 0.75f);                                    // clamped, so a broken prefs file cannot make a 1-pixel UI
    EditorTheme::Apply(9.0f);
    CHECK(EditorTheme::Scale() == 2.5f);
    EditorTheme::Apply(1.0f);
}

// ------------------------------------------------------------ icons and property grid draw
static void TestIconsAndPropertyGrid()
{
    UiEnv env;
    EditorTheme::Apply(1.0f);
    Frame([&]
    {
        ImGui::SetNextWindowPos(ImVec2(50, 50));
        ImGui::SetNextWindowSize(ImVec2(400, 600));
        ImGui::Begin("props");

        // every icon, as a drawing and as a button
        ImDrawList* dl = ImGui::GetWindowDrawList();
        float x = 70.0f;
        for (int i = static_cast<int>(Icon::Entity); i <= static_cast<int>(Icon::Shadow); ++i, x += 22.0f)
        {
            EditorIcons::Draw(dl, static_cast<Icon>(i), ImVec2(x, 90.0f), 18.0f, IM_COL32_WHITE);
            ImGui::PushID(i);
            EditorIcons::Button("##b", static_cast<Icon>(i), i % 2 == 0, "tip");
            ImGui::PopID();
            ImGui::SameLine();
        }
        ImGui::NewLine();

        // every kind of property, nothing edited: nothing reports a change
        bool anyChanged = false;
        if (PropertyGrid::Begin("all"))
        {
            DirectX::XMFLOAT3 v{ 1, 2, 3 };
            float f = 0.5f, rgb[3] = { 0.1f, 0.2f, 0.3f }, rgba[4] = { 0.1f, 0.2f, 0.3f, 1.0f };
            bool b = true;
            int i = 1;
            const char* items[] = { "a", "b", "c" };
            anyChanged |= PropertyGrid::Vec3("Position", v);
            anyChanged |= PropertyGrid::Float("Float", f);
            anyChanged |= PropertyGrid::Slider("Slider", f, 0.0f, 1.0f);
            anyChanged |= PropertyGrid::Checkbox("Check", b);
            anyChanged |= PropertyGrid::Color3("Colour3", rgb);
            anyChanged |= PropertyGrid::Color4("Colour4", rgba);
            anyChanged |= PropertyGrid::Combo("Combo", i, items, 3);
            PropertyGrid::Text("Info", "%d things", 3);
            PropertyGrid::End();
        }
        CHECK(!anyChanged);
        ImGui::End();
    });
}

// ------------------------------------------------------------ toolbar and status bar
static void TestChrome()
{
    UiEnv env;
    EditorTheme::Apply(1.0f);
    Scene scene; EditorContext ctx; Populate(scene, ctx);

    // the bars with and without a gizmo, with and without a selection, every tool, snapping on
    for (bool withGizmo : { true, false })
    {
        ctx.gizmo = withGizmo ? std::unique_ptr<IGizmo>(std::make_unique<ImGuizmoGizmo>()) : std::unique_ptr<IGizmo>(std::make_unique<NullGizmo>());
        for (int sel = 0; sel < 2; ++sel)
        {
            if (sel) ctx.selection.Set(scene.Wrap(scene.Registry().view<NameComponent>().begin()[0])); else ctx.selection.Clear();
            for (GizmoOperation op : { GizmoOperation::None, GizmoOperation::Translate, GizmoOperation::Rotate, GizmoOperation::Scale })
                for (bool snap : { false, true })
                {
                    ctx.gizmoSettings.operation = op;
                    ctx.gizmoSettings.snap = snap;
                    Frame([&] { EditorChrome::DrawToolbar(ctx); EditorChrome::DrawStatusBar(ctx, "a hint"); });
                }
        }
    }

    char text[160];
    EditorChrome::FormatStats(text, sizeof(text), 143.6f, 12, 37, 1234567);
    CHECK(std::string(text) == "144 FPS    12 entities    37 draws    1,234,567 tris");
    EditorChrome::FormatStats(text, sizeof(text), 60.0f, 0, 0, 0);
    CHECK(std::string(text) == "60 FPS    0 entities    0 draws    0 tris");
    EditorChrome::FormatStats(text, sizeof(text), 60.0f, 1, 1, 999);
    CHECK(std::string(text).find("999 tris") != std::string::npos);
    EditorChrome::FormatStats(text, sizeof(text), 60.0f, 1, 1, 1000);
    CHECK(std::string(text).find("1,000 tris") != std::string::npos);
}

static void TestRendererPanel()
{
    UiEnv env;
    EditorTheme::Apply(1.0f);
    Renderer::SetEnvironment(RenderEnvironment());
    const RenderEnvironment before = Renderer::GetEnvironment();

    bool open = true;
    for (int i = 0; i < 4; ++i)
        Frame([&] { RendererPanel::OnImGui(&open); });
    // looking at the panel changes nothing
    const RenderEnvironment after = Renderer::GetEnvironment();
    CHECK(after.exposure == before.exposure && after.gamma == before.gamma && after.shadows.enabled == before.shadows.enabled &&
          after.shadows.cascadeCount == before.shadows.cascadeCount && after.lightCulling == before.lightCulling);

    open = false;
    Frame([&] { RendererPanel::OnImGui(&open); });                              // closed: draws nothing, no crash
}

// ------------------------------------------------------------ the hierarchy, used like a user would
struct HierarchyRig
{
    UiEnv env;
    Scene scene;
    EditorContext ctx;
    HierarchyPanel panel;
    bool open = true;
    Entity alpha, beta, gamma, delta;

    static constexpr float kX = 50, kY = 50, kW = 300, kH = 500;

    HierarchyRig()
    {
        EditorTheme::Apply(1.0f);
        ctx.scene = &scene;
        alpha = scene.CreateEntity("Alpha");
        MeshRendererComponent mr; mr.mesh = MakeCube();
        alpha.Add<MeshRendererComponent>(mr);
        beta  = scene.CreateEntity("Beta");
        gamma = scene.CreateEntity("Gamma");
        delta = scene.CreateEntity("Delta", gamma);
        for (int i = 0; i < 4; ++i) Draw();
    }

    void Draw()
    {
        Frame([&]
        {
            ImGui::SetNextWindowPos(ImVec2(kX, kY));
            ImGui::SetNextWindowSize(ImVec2(kW, kH));
            panel.OnImGui(ctx, &open);
        });
    }

    ImGuiIO& IO() { return ImGui::GetIO(); }
    void MoveTo(float x, float y) { IO().AddMousePosEvent(x, y); Draw(); Draw(); }
    void Click(float x, float y) { MoveTo(x, y); IO().AddMouseButtonEvent(0, true); Draw(); IO().AddMouseButtonEvent(0, false); Draw(); }
    void DoubleClick(float x, float y)
    {
        // Clicks within 0.3 s of each other at the same spot COUNT as one multi-click (3, 4, ... 18 after a scan with many
        // clicks), and only a count of exactly 2 is a double click. Let the earlier ones expire, as a person would.
        for (int i = 0; i < 30; ++i) Draw();
        MoveTo(x, y);
        for (int i = 0; i < 2; ++i) { IO().AddMouseButtonEvent(0, true); Draw(); IO().AddMouseButtonEvent(0, false); Draw(); }
    }
    void Drag(float x0, float y0, float x1, float y1)
    {
        MoveTo(x0, y0);
        IO().AddMouseButtonEvent(0, true); Draw();
        for (int i = 1; i <= 10; ++i) { IO().AddMousePosEvent(x0 + (x1 - x0) * i / 10.0f, y0 + (y1 - y0) * i / 10.0f); Draw(); }
        Draw();
        IO().AddMouseButtonEvent(0, false); Draw(); Draw();
    }
    void Key(ImGuiKey key) { IO().AddKeyEvent(key, true); Draw(); IO().AddKeyEvent(key, false); Draw(); }
    void Type(const char* text) { IO().AddInputCharactersUTF8(text); Draw(); Draw(); }

    // The y of the row of 'e': scan down the list with clicks until the selection becomes 'e'. (The tests know nothing of
    // the layout, so they cannot be broken by a change of it, only by a change of behaviour.)
    float RowY(Entity e)
    {
        for (float y = kY + 50.0f; y < kY + kH - 40.0f; y += 3.0f)
        {
            ctx.selection.Clear();
            Click(kX + 120.0f, y);
            if (ctx.selection.Is(e)) return y;
        }
        return -1.0f;
    }
};

static void TestHierarchyRowsOrderAndSelection()
{
    HierarchyRig r;
    const float yA = r.RowY(r.alpha), yB = r.RowY(r.beta), yG = r.RowY(r.gamma);
    CHECK(yA > 0 && yB > 0 && yG > 0);
    CHECK(yA < yB && yB < yG);                                           // the order of the tree
    CHECK(yB - yA > 20.0f && yB - yA < 50.0f && std::fabs((yG - yB) - (yB - yA)) < 5.0f);   // a regular row height
    CHECK(r.RowY(r.delta) < 0.0f);                                       // the child is hidden while Gamma is collapsed

    // click on empty space below the rows clears the selection
    r.Click(HierarchyRig::kX + 120.0f, HierarchyRig::kY + HierarchyRig::kH - 30.0f);
    CHECK(!r.ctx.selection.Has());

    // the arrow of a parent expands it and the child appears below it
    r.Click(HierarchyRig::kX + 10.0f + 12.0f, yG);                        // the arrow cell: left edge of the row
    const float yD = r.RowY(r.delta);
    CHECK(yD > yG);
}

static void TestHierarchyEyeTogglesVisibility()
{
    HierarchyRig r;
    const float yA = r.RowY(r.alpha);
    CHECK(yA > 0);
    MeshRendererComponent& mr = r.alpha.Get<MeshRendererComponent>();
    CHECK(mr.visible);
    const float eyeX = HierarchyRig::kX + HierarchyRig::kW - 28.0f;       // the right end of the row
    r.Click(eyeX, yA);
    CHECK(!mr.visible);                                                   // hidden
    r.Click(eyeX, yA);
    CHECK(mr.visible);                                                    // shown again
    // the eye is only on meshes: the same click on an entity without a mesh does nothing to it
    const float yB = r.RowY(r.beta);
    r.Click(eyeX, yB);
    CHECK(!r.beta.Has<MeshRendererComponent>());
}

static void TestHierarchyRename()
{
    HierarchyRig r;
    // double click on a leaf: rename in place; typing replaces the name, Enter keeps it
    const float yB = r.RowY(r.beta);
    CHECK(yB > 0);
    r.DoubleClick(HierarchyRig::kX + 120.0f, yB);
    r.Draw(); r.Draw();
    r.Type("Zed");
    r.Key(ImGuiKey_Enter);
    CHECK(r.beta.Get<NameComponent>().name == "Zed");

    // F2 on the selection does the same; Escape cancels
    const float yA = r.RowY(r.alpha);
    r.Key(ImGuiKey_F2);
    r.Draw(); r.Draw();
    r.Type("Nope");
    r.Key(ImGuiKey_Escape);
    CHECK(r.alpha.Get<NameComponent>().name == "Alpha");                  // cancelled: unchanged
    r.Key(ImGuiKey_F2);
    r.Draw(); r.Draw();
    r.Type("Eff");
    r.Key(ImGuiKey_Enter);
    CHECK(yA > 0 && r.alpha.Get<NameComponent>().name == "Eff");

    // an empty name is not accepted (the old one stays)
    r.Key(ImGuiKey_F2);
    r.Draw(); r.Draw();
    r.IO().AddKeyEvent(ImGuiKey_Backspace, true); r.Draw(); r.IO().AddKeyEvent(ImGuiKey_Backspace, false); r.Draw();
    r.Key(ImGuiKey_Enter);
    CHECK(!r.alpha.Get<NameComponent>().name.empty());
}

static void TestHierarchyDeleteDuplicate()
{
    HierarchyRig r;
    const size_t start = r.scene.EntityCount();

    const float yA = r.RowY(r.alpha);
    CHECK(yA > 0);
    r.IO().AddKeyEvent(ImGuiMod_Ctrl, true);
    r.Key(ImGuiKey_D);                                                    // Ctrl+D
    r.IO().AddKeyEvent(ImGuiMod_Ctrl, false);
    r.Draw();
    CHECK(r.scene.EntityCount() == start + 1);
    CHECK(r.ctx.selection.Has() && r.ctx.selection.Get().Get<NameComponent>().name == "Alpha (Copy)");   // the copy is selected
    CHECK(r.ctx.selection.Get().Has<MeshRendererComponent>());

    // Delete removes the selection (and a parent takes its children with it)
    const float yG = r.RowY(r.gamma);
    CHECK(yG > 0);
    r.Key(ImGuiKey_Delete);
    CHECK(!r.gamma.IsValid() && !r.delta.IsValid());
    CHECK(r.scene.EntityCount() == start + 1 - 2);
    CHECK(!r.ctx.selection.Has());
}

static void TestHierarchyDragToReparent()
{
    HierarchyRig r;
    const float yA = r.RowY(r.alpha), yB = r.RowY(r.beta);
    CHECK(yA > 0 && yB > 0);

    // Alpha is moved in the world before, to prove that parenting keeps it where it is
    r.alpha.Get<TransformComponent>().position = { 5, 0, 0 };
    r.beta.Get<TransformComponent>().position = { 0, 10, 0 };
    r.beta.Get<TransformComponent>().scale = { 2, 2, 2 };
    TransformSystem::Update(r.scene);
    const DirectX::XMFLOAT3 before = r.alpha.Get<TransformComponent>().WorldPosition();

    r.Drag(HierarchyRig::kX + 120.0f, yA, HierarchyRig::kX + 120.0f, yB);  // drop Alpha on Beta
    CHECK(r.scene.GetParent(r.alpha) == r.beta);
    TransformSystem::Update(r.scene);
    const DirectX::XMFLOAT3 after = r.alpha.Get<TransformComponent>().WorldPosition();
    CHECK(Distance(before, after) < 1e-3f);                               // it did not move

    // drag it to the empty area: a root again, still in place
    r.Drag(HierarchyRig::kX + 150.0f, HierarchyRig::kY + 50.0f + 3 * 29.0f, HierarchyRig::kX + 120.0f, HierarchyRig::kY + HierarchyRig::kH - 30.0f);
    // (the child row position is found by scanning in the next check rather than assumed)
    if (r.scene.GetParent(r.alpha).IsValid())
    {
        const float yChild = r.RowY(r.alpha);
        if (yChild > 0) r.Drag(HierarchyRig::kX + 150.0f, yChild, HierarchyRig::kX + 120.0f, HierarchyRig::kY + HierarchyRig::kH - 30.0f);
    }
    TransformSystem::Update(r.scene);
    CHECK(!r.scene.GetParent(r.alpha).IsValid());
    CHECK(Distance(before, r.alpha.Get<TransformComponent>().WorldPosition()) < 1e-3f);

    // a parent cannot be dropped on its own child
    TransformSystem::Update(r.scene);
    const float yG = r.RowY(r.gamma);
    r.Click(HierarchyRig::kX + 22.0f, yG);                                // expand Gamma
    const float yD = r.RowY(r.delta);
    if (yG > 0 && yD > 0) r.Drag(HierarchyRig::kX + 120.0f, yG, HierarchyRig::kX + 150.0f, yD);
    CHECK(!r.scene.GetParent(r.gamma).IsValid());                         // refused: it would be a cycle
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "hierarchy panel: rows, reveal, every selection", TestHierarchyPanel },
        { "inspector panel: every entity kind",             TestInspectorPanel },
        { "viewport panel: size request, tools, hidden",    TestViewportPanel },
        { "viewport panel: camera, outline, pick, frame",   TestViewportWithCamera },
        { "imgui: captures while a button is held",         TestImGuiCapturesWhileAButtonIsHeld },
        { "camera: input gate",                             TestCameraInputGate },
        { "theme: scale applies, never compounds",          TestThemeScaling },
        { "icons and property grid draw",                   TestIconsAndPropertyGrid },
        { "toolbar and status bar",                         TestChrome },
        { "renderer panel: looking changes nothing",        TestRendererPanel },
        { "hierarchy: row order, selection, expand",        TestHierarchyRowsOrderAndSelection },
        { "hierarchy: the eye toggles visibility",          TestHierarchyEyeTogglesVisibility },
        { "hierarchy: rename (double click, F2, Esc)",      TestHierarchyRename },
        { "hierarchy: Ctrl+D and Delete",                   TestHierarchyDeleteDuplicate },
        { "hierarchy: drag to reparent, keeps position",    TestHierarchyDragToReparent },
        { "gizmo: a drag moves along the dragged axis",     TestGizmoTranslatesAlongTheDraggedAxis },
        { "gizmo: a child under a rotated, scaled parent",  TestViewportGizmoMovesAChildUnderATransformedParent },
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
