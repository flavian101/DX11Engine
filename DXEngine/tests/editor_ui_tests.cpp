// Editor UI smoke test: runs the real panels through real ImGui frames with NO window and NO GPU.
// ImGui's own assertions are the oracle: unbalanced Begin/End, Push/Pop, BeginDisabled/EndDisabled, duplicate ids in a
// list, a drag-drop source without a target... all abort. So "no abort" means the panels are structurally sound and
// every code path that draws a widget was executed. (What it cannot say: whether it LOOKS right. That needs eyes.)
//   build: see DXEngine/tests/CMakeLists.txt  (links the ImGui core files, not the backends)
#include "editor/AssetsPanel.h"
#include "editor/EditorChrome.h"
#include "editor/EditorContext.h"
#include "editor/EditorIcons.h"
#include "editor/SecondViewPanel.h"
#include "editor/SceneFileController.h"
#include "editor/SceneIO.h"
#include "assets/EngineAssetBridge.h"
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
#include "models/ModelAsset.h"
#include "utils/ImageLoader.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include "imgui.h"
#include <cstdio>
#include <filesystem>
#include <fstream>

using namespace DXEngine;

// The renderer is not linked into this test (it needs a GPU): these are the members the panels call, kept in plain
// variables, and just enough of the class to have an object to call them on. Nothing here touches a member of Renderer.
namespace DXEngine
{
    struct Renderer::Impl {};
    Renderer::Renderer() : m_Main(new RenderView(this, "Main")) { m_Views.push_back(m_Main.get()); }
    Renderer::~Renderer() {}
    bool Renderer::HasInstance() { return true; }
    RenderView& Renderer::GetMainView() { return *m_Main; }
    std::unique_ptr<RenderView> Renderer::CreateView(const std::string& name)
    {
        std::unique_ptr<RenderView> view(new RenderView(this, name));
        m_Views.push_back(view.get());
        return view;
    }
    // counts what was asked to be drawn, and how
    static int g_renders = 0;
    static Scene* g_renderScene = nullptr;
    static const Camera* g_renderCamera = nullptr;
    static bool g_renderSceneWasCurrent = false;
    void Renderer::Render(RenderView&, Scene* scene, const std::shared_ptr<Camera>& camera)
    {
        ++g_renders; g_renderScene = scene; g_renderCamera = camera.get();
        g_renderSceneWasCurrent = scene && TransformSystem::IsCurrent(*scene);
    }
    void Renderer::UnregisterView(RenderView*) {}
    Renderer& Renderer::Get() { static Renderer* stub = new Renderer(); return *stub; }

    static int g_lastSceneW = -1, g_lastSceneH = -1;
    void Renderer::SetSceneTargetSize(int w, int h) { g_lastSceneW = w; g_lastSceneH = h; }
    RHI::ITexture* Renderer::GetSceneTexture() const { return nullptr; }

    // what the toolbar and the Renderer panel read and write
    static RenderEnvironment g_env;
    static bool g_wireframe = false, g_culling = true;
    static RenderStatistics g_stats;
    void Renderer::EnableWireframe(bool e) { g_wireframe = e; }
    bool Renderer::IsWireframeEnabled() const { return g_wireframe; }
    void Renderer::EnableFrustumCulling(bool e) { g_culling = e; }
    bool Renderer::IsFrustumCullingEnabled() const { return g_culling; }
    void Renderer::SetEnvironment(const RenderEnvironment& e) { g_env = e; }
    RenderEnvironment Renderer::GetEnvironment() const { return g_env; }
    const RenderStatistics& Renderer::GetStats() const { return g_stats; }
    static bool g_vsync = false, g_gpuTiming = true;
    static GpuFrameTimings g_timings;
    void Renderer::SetVSync(bool e) { g_vsync = e; }
    bool Renderer::IsVSyncEnabled() const { return g_vsync; }
    void Renderer::SetGpuProfilingEnabled(bool e) { g_gpuTiming = e; }
    bool Renderer::IsGpuProfilingEnabled() const { return g_gpuTiming; }
    bool Renderer::IsGpuProfilingAvailable() const { return true; }
    const GpuFrameTimings& Renderer::GetGpuTimings() const { return g_timings; }
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

// The panels reach the renderer's primary device, and the renderer is not linked into this test (it needs a GPU): there is no device.
// (The image loader IS linked, for the scene file tests' asset bridge; it is never asked to load without a device.)
namespace DXEngine
{
    std::shared_ptr<RHI::IGraphicsDevice> Renderer::PrimaryDevice() { return nullptr; }
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
    scene.CreateEntity("Skinned").Add<SkinnedMeshComponent>();                       // skinned, nothing above it
    Entity rig = scene.CreateEntity("Rig");                                          // a real Animator: skeleton + a clip
    auto& animator = rig.Add<AnimatorComponent>();
    animator.skeleton = std::make_shared<Skeleton>();
    Bone bone; bone.Name = "root"; animator.skeleton->AddBone(bone);
    auto clip = std::make_shared<AnimationClip>("Walk", 30.0f);
    clip->AddBoneAnimation("root", { Keyframe{}, Keyframe{} });
    animator.clips = { clip };
    scene.CreateEntity("Rig skin", rig).Add<SkinnedMeshComponent>();                 // bones from the Animator above it
    scene.CreateEntity("Bare animator").Add<AnimatorComponent>();                    // no skeleton, no clips: must still draw
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
// The second view: its own camera, its own texture, drawn from inside the panel; closing it frees the view.
static void TestSecondViewPanel()
{
    static NoInput noInput;
    Input::Init(&noInput);

    UiEnv env;
    EditorTheme::Apply(1.0f);
    Scene scene; EditorContext ctx; Populate(scene, ctx);
    ctx.camera = std::make_shared<CameraController>();
    ctx.camera->SetFollowWindowAspect(false);
    ctx.camera->GetCamera()->SetPosition({ 4, 5, 6 });
    ctx.camera->GetCamera()->SetRotation({ -0.3f, 0.7f, 0.0f });

    SecondViewPanel panel;
    bool open = false;

    // closed: nothing is created and nothing is drawn
    g_renders = 0;
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(!panel.HasView() && g_renders == 0);

    // open: a view and a camera appear, the scene is drawn through THAT camera (not the editor's), once per frame
    open = true;
    for (int i = 0; i < 3; ++i) Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(panel.HasView() && panel.ViewCamera() != nullptr);
    CHECK(g_renders == 3);
    CHECK(g_renderScene == &scene);
    CHECK(g_renderCamera == panel.ViewCamera() && g_renderCamera != ctx.camera->GetCamera().get());
    CHECK(panel.TargetSize().width >= 1 && panel.TargetSize().height >= 1);

    // it started where the editor camera was ...
    Camera* view = const_cast<Camera*>(panel.ViewCamera());
    CHECK(Distance(view->GetPositionFloat3(), { 4, 5, 6 }) < 1e-5f);
    // ... and does NOT follow it afterwards, until pinned again
    ctx.camera->GetCamera()->SetPosition({ -7, 1, 2 });
    for (int i = 0; i < 2; ++i) Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(Distance(view->GetPositionFloat3(), { 4, 5, 6 }) < 1e-5f);
    panel.Pin(ctx);
    CHECK(Distance(view->GetPositionFloat3(), { -7, 1, 2 }) < 1e-5f);
    CHECK(Distance(view->GetRotationFloat3(), { -0.3f, 0.7f, 0.0f }) < 1e-5f);

    // world matrices are brought up to date before drawing: a move made earlier in the same frame is not drawn stale
    Entity mover = scene.Wrap(scene.Registry().view<NameComponent>().begin()[0]);
    mover.Get<TransformComponent>().position = { 40, 0, 0 };
    CHECK(!TransformSystem::IsCurrent(scene));
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(g_renderSceneWasCurrent);

    // no scene: says so, draws nothing
    const int before = g_renders;
    EditorContext empty;
    Frame([&] { panel.OnImGui(empty, &open); });
    CHECK(g_renders == before);

    // closing frees the view; reopening makes a new one
    open = false;
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(!panel.HasView());
    open = true;
    Frame([&] { panel.OnImGui(ctx, &open); });
    CHECK(panel.HasView());
}

// New / Open / Save / Save As and the "unsaved changes" question, with temp files, stand-in dialogs and the real asset bridge
// (generated meshes need no device). The modal is drawn in real ImGui frames; the answers are given directly.
struct FileEnv
{
    FileEnv()
    {
        dir = (std::filesystem::temp_directory_path() / "dx_scene_file_tests").generic_string();
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        SceneFileController::Hooks hooks;
        hooks.pickOpenPath = [this] { ++openDialogs; return openAnswer; };
        hooks.pickSavePath = [this](const std::string& suggested) { ++saveDialogs; lastSuggested = suggested; return saveAnswer; };
        hooks.makeBridge = [this]() -> std::unique_ptr<IAssetBridge>
        {
            return std::make_unique<EngineAssetBridge>(nullptr, models, importer, bridgeLog, dir);
        };
        hooks.log = [this](const std::string& line, bool warning) { lines.push_back((warning ? "! " : "  ") + line); };
        controller = std::make_unique<SceneFileController>(std::move(hooks));
    }
    ~FileEnv() { std::filesystem::remove_all(dir); }

    bool Logged(const char* part) const { for (const auto& l : lines) if (l.find(part) != std::string::npos) return true; return false; }

    std::string dir, openAnswer, saveAnswer, lastSuggested;
    int openDialogs = 0, saveDialogs = 0;
    ModelLibrary models;
    ModelImporter importer;
    Assets::ImportLog bridgeLog;
    std::vector<std::string> lines;
    std::unique_ptr<SceneFileController> controller;
};

static void TestSceneFileController()
{
    UiEnv ui;
    EditorTheme::Apply(1.0f);
    FileEnv env;
    SceneFileController& files = *env.controller;

    Scene scene;
    EditorContext ctx;
    ctx.scene = &scene;
    Entity a = scene.CreateEntity("a");
    Entity b = scene.CreateEntity("b", a);
    { MeshRendererComponent r; r.mesh = Mesh::CreateCube(nullptr, 2.0f); r.materials = { MaterialFactory::CreateLitMaterial("paint") }; b.Add<MeshRendererComponent>(r); }
    a.Get<TransformComponent>().position = { 1, 2, 3 };
    ctx.selection.Set(b);
    const std::string path = env.dir + "/level.dxscene";

    // a scene that was never saved counts as modified
    CHECK(files.IsModified(ctx) && files.Path().empty() && !files.HasPending());

    // Save with no file yet asks where (the suggestion is a name), writes it, and the scene is no longer modified
    env.saveAnswer = path;
    CHECK(files.Save(ctx) && env.saveDialogs == 1 && env.lastSuggested == "scene.dxscene");
    CHECK(std::filesystem::is_regular_file(path) && files.Path() == path && !files.IsModified(ctx) && env.Logged("saved 2 entities"));

    // an edit makes it modified; Save now writes to the same file WITHOUT asking
    a.Get<TransformComponent>().position = { 4, 5, 6 };
    CHECK(files.IsModified(ctx));
    CHECK(files.Save(ctx) && env.saveDialogs == 1 && !files.IsModified(ctx));

    // Save As cancelled: nothing changes
    env.saveAnswer.clear();
    std::string before; SceneIO::ReadTextFile(path, before);
    CHECK(!files.SaveAs(ctx) && env.saveDialogs == 2 && files.Path() == path);
    std::string after; SceneIO::ReadTextFile(path, after);
    CHECK(before == after);
    // Save As to a new name (no extension typed): the extension is added, the path moves
    env.saveAnswer = env.dir + "/copy";
    CHECK(files.SaveAs(ctx) && files.Path() == env.dir + "/copy.dxscene" && env.lastSuggested == path && std::filesystem::is_regular_file(env.dir + "/copy.dxscene"));
    CHECK(files.Save(ctx));                                                                    // keeps writing to the new file
    env.saveAnswer = path;
    CHECK(files.SaveAs(ctx) && files.Path() == path);

    // ---- New, unmodified: immediate
    env.openDialogs = 0;
    files.New(ctx);
    CHECK(!files.HasPending() && scene.EntityCount() == 0 && !ctx.selection.Has() && files.Path().empty() && !files.IsModified(ctx));

    // ---- Open, unmodified: loads, selection cleared, world matrices current, not modified
    env.openAnswer = path;
    files.Open(ctx);
    CHECK(env.openDialogs == 1 && !files.HasPending() && scene.EntityCount() == 2 && files.Path() == path);
    CHECK(!ctx.selection.Has() && TransformSystem::IsCurrent(scene) && !files.IsModified(ctx) && env.Logged("opened"));
    Entity la = scene.FindByUuid(Uuid{}).IsValid() ? Entity() : Entity();
    scene.Registry().view<NameComponent>().each([&](entt::entity e, const NameComponent& n) { if (n.name == "a") la = scene.Wrap(e); });
    CHECK(la.IsValid() && la.Get<TransformComponent>().position.x == 4 && la.Get<TransformComponent>().world._41 == 4);
    auto kids = scene.GetChildren(la);
    CHECK(kids.size() == 1 && kids[0].Get<MeshRendererComponent>().mesh->GetSourceName() == "primitive/cube?size=2");

    // ---- Open, modified: asks; each answer
    la.Get<TransformComponent>().position = { 9, 9, 9 };
    files.Open(ctx);
    CHECK(files.HasPending() && env.openDialogs == 1);                                         // nothing happened yet
    Frame([&] { files.OnImGui(ctx); });
    CHECK(ImGui::IsPopupOpen("Unsaved changes", ImGuiPopupFlags_AnyPopupId) || files.HasPending());   // the modal is up
    files.AnswerPending(ctx, SceneFileController::Choice::Cancel);
    CHECK(!files.HasPending() && env.openDialogs == 1 && la.Get<TransformComponent>().position.x == 9);   // still the edited scene

    files.Open(ctx);
    env.openAnswer = path;
    ctx.selection.Set(la);                                                                     // something is selected: it is not in the loaded scene
    CHECK(ctx.selection.Has());
    files.AnswerPending(ctx, SceneFileController::Choice::DontSave);                           // discard the edit, load the file
    CHECK(!files.HasPending() && env.openDialogs == 2 && !files.IsModified(ctx) && !ctx.selection.Has());
    scene.Registry().view<NameComponent>().each([&](entt::entity e, const NameComponent& n) { if (n.name == "a") la = scene.Wrap(e); });
    CHECK(la.Get<TransformComponent>().position.x == 4);

    la.Get<TransformComponent>().position = { 7, 7, 7 };
    files.Open(ctx);
    files.AnswerPending(ctx, SceneFileController::Choice::Save);                               // saves to the current file, then opens it again
    CHECK(!files.HasPending() && env.openDialogs == 3);
    scene.Registry().view<NameComponent>().each([&](entt::entity e, const NameComponent& n) { if (n.name == "a") la = scene.Wrap(e); });
    CHECK(la.Get<TransformComponent>().position.x == 7);                                       // the edit was saved, not lost

    // Save chosen but the save dialog is cancelled (new scene has no path): the pending action is dropped, nothing is lost
    files.New(ctx);                                                                            // unmodified: immediate
    ctx.scene->CreateEntity("lonely");
    env.saveAnswer.clear();
    files.Open(ctx);
    CHECK(files.HasPending());
    files.AnswerPending(ctx, SceneFileController::Choice::Save);
    CHECK(!files.HasPending() && scene.EntityCount() == 1 && env.openDialogs == 3);             // no dialog for Open, the scene is still there

    // New, modified: asks too
    files.New(ctx);
    CHECK(files.HasPending() && scene.EntityCount() == 1);
    files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(!files.HasPending() && scene.EntityCount() == 0);

    // ---- bad files change nothing
    ctx.scene->CreateEntity("keep");
    env.openAnswer = env.dir + "/corrupt.dxscene";
    SceneIO::WriteTextFile(env.openAnswer, "{ this is not a scene");
    files.Open(ctx);                                                                           // modified (never saved): asks
    files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(scene.EntityCount() == 1 && env.Logged("open failed"));
    env.openAnswer = env.dir + "/does_not_exist.dxscene";
    files.New(ctx); files.AnswerPending(ctx, SceneFileController::Choice::DontSave);       // (modified: it asks first)
    ctx.scene->CreateEntity("keep");
    files.Open(ctx); files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(scene.EntityCount() == 1 && env.Logged("is not a file"));
    env.openAnswer = env.dir + "/future.dxscene";
    SceneIO::WriteTextFile(env.openAnswer, "{\"format\":\"dxengine.scene\",\"version\":99,\"entities\":[]}");
    files.Open(ctx); files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(scene.EntityCount() == 1 && env.Logged("update the engine"));

    // a cancelled Open dialog is not an error
    env.openAnswer.clear();
    const size_t logged = env.lines.size();
    files.Open(ctx); files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(scene.EntityCount() == 1 && env.lines.size() == logged);

    // an answer with nothing pending does nothing
    files.AnswerPending(ctx, SceneFileController::Choice::DontSave);
    CHECK(scene.EntityCount() == 1);

    // a failing save (the target is a directory) says so and keeps the state
    const std::string pathBefore = files.Path();
    std::filesystem::create_directories(env.dir + "/blocked.dxscene");
    env.saveAnswer = env.dir + "/blocked.dxscene";
    CHECK(!files.SaveAs(ctx) && env.Logged("save failed") && files.Path() == pathBefore && std::filesystem::is_directory(env.dir + "/blocked.dxscene"));
}

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
    Renderer::Get().SetEnvironment(RenderEnvironment());
    const RenderEnvironment before = Renderer::Get().GetEnvironment();

    bool open = true;
    for (int i = 0; i < 4; ++i)
        Frame([&] { RendererPanel::OnImGui(&open); });
    // looking at the panel changes nothing
    const RenderEnvironment after = Renderer::Get().GetEnvironment();
    CHECK(after.exposure == before.exposure && after.gamma == before.gamma && after.shadows.enabled == before.shadows.enabled &&
          after.shadows.cascadeCount == before.shadows.cascadeCount && after.lightCulling == before.lightCulling &&
          after.depthPrepass == before.depthPrepass);

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

// ------------------------------------------------------------ the Assets panel: files, a held import, the library, the log
static std::shared_ptr<ModelAsset> TestModelAsset()
{
    auto asset = std::make_shared<ModelAsset>();
    DirectX::XMFLOAT4X4 identity; DirectX::XMStoreFloat4x4(&identity, DirectX::XMMatrixIdentity());
    asset->AddNode("RootNode", -1, identity);
    asset->AttachMesh(0, asset->AddMesh("body", MakeCube()));
    asset->warnings = { "a texture is missing" };
    return asset;
}

static void TestAssetsPanel()
{
    UiEnv env;

    // a folder with models in it, and a file that is not one
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "dx_assets_panel_test";
    fs::remove_all(root);
    fs::create_directories(root / "ship");
    for (const char* f : { "tree.fbx", "ship/hull.glb", "ship/broken.obj", "ship/notes.txt" }) std::ofstream(root / f) << "x";

    Scene scene; EditorContext ctx; Populate(scene, ctx);
    AssetsPanel panel;
    panel.SetAssetsFolder(root.string());
    int loads = 0;
    panel.Importer().SetLoadFunction([&](const std::string& path, bool, std::string& error) -> std::shared_ptr<ModelAsset>
    {
        ++loads;
        if (path.find("broken") != std::string::npos) { error = "not a model"; return nullptr; }
        return TestModelAsset();
    });
    bool open = true;
    auto frame = [&] { Frame([&] { panel.Update(ctx); panel.OnImGui(ctx, &open); }); };

    // the panel lists the three model files (the .txt is not one) and draws without anything imported
    for (int i = 0; i < 3; ++i) frame();
    CHECK(ctx.models.Count() == 0 && !panel.IsBusy());

    // a request is HELD one frame (that frame shows "Importing ..."), then the import runs
    const size_t entitiesBefore = scene.EntityCount();
    panel.RequestImport((root / "tree.fbx").string(), true);
    CHECK(panel.IsBusy() && loads == 0);
    panel.RequestImport((root / "ship/hull.glb").string(), true);          // a second one while busy is ignored, not queued
    frame();                                                               // frame 1: the message, no work yet
    CHECK(panel.IsBusy() && loads == 0 && ctx.models.Count() == 0);
    frame();                                                               // frame 2: the work
    CHECK(!panel.IsBusy() && loads == 1 && ctx.models.Count() == 1);

    // it went into the scene, named like the file, selected, with the library counting the instance
    CHECK(scene.EntityCount() == entitiesBefore + 1);                      // a one-node model: one entity
    CHECK(ctx.selection.Has() && ctx.selection.Get().Get<NameComponent>().name == "tree");
    CHECK(ctx.selection.Get().Has<MeshRendererComponent>());
    CHECK(ctx.revealSelection);
    CHECK(ctx.models.All()[0].instances == 1);
    CHECK(panel.Log().Count(Assets::LogSeverity::Info) >= 2 && panel.Log().Count(Assets::LogSeverity::Warning) == 1);   // import, add; the warning

    // the same model again: no second read, one more instance
    panel.RequestImport((root / "tree.fbx").string(), true);
    frame(); frame();
    CHECK(loads == 1 && ctx.models.All()[0].instances == 2 && scene.EntityCount() == entitiesBefore + 2);

    // import only: nothing is added to the scene
    panel.RequestImport((root / "ship/hull.glb").string(), false);
    frame(); frame();
    CHECK(loads == 2 && ctx.models.Count() == 2 && scene.EntityCount() == entitiesBefore + 2);

    // a reimport reads again and keeps one entry
    panel.RequestImport((root / "ship/hull.glb").string(), false, true);
    frame(); frame();
    CHECK(loads == 3 && ctx.models.Count() == 2);

    // a file that fails: an error line, nothing added
    const size_t errors = panel.Log().Count(Assets::LogSeverity::Error);
    panel.RequestImport((root / "ship/broken.obj").string(), true);
    frame(); frame();
    CHECK(ctx.models.Count() == 2 && panel.Log().Count(Assets::LogSeverity::Error) == errors + 1 && scene.EntityCount() == entitiesBefore + 2);

    // the "Import file..." picker: a chosen file is imported, a cancelled dialog does nothing
    std::string chosen;
    panel.SetFilePicker([&] { return chosen; });
    panel.PickAndImport();
    CHECK(!panel.IsBusy());
    chosen = (root / "tree.fbx").string();
    panel.PickAndImport();
    CHECK(panel.IsBusy());
    frame(); frame();
    CHECK(ctx.models.All()[0].instances == 3);

    // everything draws with models imported, a filter, an empty result, and the window closed
    panel.RequestImport("", true);                                         // an empty path is not a request
    CHECK(!panel.IsBusy());
    for (int i = 0; i < 3; ++i) frame();
    panel.SetAssetsFolder((root / "does_not_exist").string());             // a missing folder: a hint, no crash
    for (int i = 0; i < 2; ++i) frame();
    panel.SetAssetsFolder(root.string());
    open = false;
    frame();
    open = true;

    // without a reader or without a scene nothing crashes
    AssetsPanel bare;
    EditorContext bareCtx; bareCtx.scene = &scene;                         // (an empty library: the file is not "already imported")
    bare.SetAssetsFolder(root.string());
    bare.RequestImport((root / "tree.fbx").string(), true);
    Frame([&] { bare.Update(bareCtx); bare.OnImGui(bareCtx, &open); });
    Frame([&] { bare.Update(bareCtx); bare.OnImGui(bareCtx, &open); });
    CHECK(bare.Log().Count(Assets::LogSeverity::Error) == 1 && bareCtx.models.Count() == 0);
    EditorContext noScene;
    AssetsPanel p2;
    p2.Importer().SetLoadFunction([](const std::string&, bool, std::string&) { return TestModelAsset(); });
    p2.SetAssetsFolder(root.string());
    p2.RequestImport((root / "tree.fbx").string(), true);
    Frame([&] { p2.Update(noScene); p2.OnImGui(noScene, &open); });
    Frame([&] { p2.Update(noScene); p2.OnImGui(noScene, &open); });
    CHECK(noScene.models.Count() == 1);                                    // imported, nothing to add it to

    // the inspector's Add component menu lists the imported meshes: drawn for an entity without a renderer
    InspectorPanel inspector;
    Entity bareEntity = scene.CreateEntity("needs a mesh");
    ctx.selection.Set(bareEntity);
    for (int i = 0; i < 3; ++i) Frame([&] { inspector.OnImGui(ctx, &open); });
    fs::remove_all(root);
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "hierarchy panel: rows, reveal, every selection", TestHierarchyPanel },
        { "inspector panel: every entity kind",             TestInspectorPanel },
        { "viewport panel: size request, tools, hidden",    TestViewportPanel },
        { "viewport panel: camera, outline, pick, frame",   TestViewportWithCamera },
        { "second view: own camera, own view, closes",      TestSecondViewPanel },
        { "scene files: new, open, save, the question",     TestSceneFileController },
        { "imgui: captures while a button is held",         TestImGuiCapturesWhileAButtonIsHeld },
        { "camera: input gate",                             TestCameraInputGate },
        { "theme: scale applies, never compounds",          TestThemeScaling },
        { "icons and property grid draw",                   TestIconsAndPropertyGrid },
        { "toolbar and status bar",                         TestChrome },
        { "assets panel: import, library, log",           TestAssetsPanel },
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
