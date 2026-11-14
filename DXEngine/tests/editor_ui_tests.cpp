// Editor UI smoke test: runs the real panels through real ImGui frames with NO window and NO GPU.
// ImGui's own assertions are the oracle: unbalanced Begin/End, Push/Pop, BeginDisabled/EndDisabled, duplicate ids in a
// list, a drag-drop source without a target... all abort. So "no abort" means the panels are structurally sound and
// every code path that draws a widget was executed. (What it cannot say: whether it LOOKS right. That needs eyes.)
//   build: see DXEngine/tests/CMakeLists.txt  (links the ImGui core files, not the backends)
#include "editor/EditorContext.h"
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

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "hierarchy panel: rows, reveal, every selection", TestHierarchyPanel },
        { "inspector panel: every entity kind",             TestInspectorPanel },
        { "viewport panel: size request, tools, hidden",    TestViewportPanel },
        { "viewport panel: camera, outline, pick, frame",   TestViewportWithCamera },
        { "imgui: captures while a button is held",         TestImGuiCapturesWhileAButtonIsHeld },
        { "camera: input gate",                             TestCameraInputGate },
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
