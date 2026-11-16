// ModelAsset, Scene::Instantiate and the Animator, without a GPU: the asset's node tree becomes entities, a skinned model
// gets an Animator with a playback of its own, and AnimationSystem steps it.
#include "models/ModelAsset.h"
#include "scene/AnimationSystem.h"
#include "scene/Scene.h"
#include "scene/SceneFactory.h"
#include "scene/TransformSystem.h"
#include "utils/Mesh/Mesh.h"
#include "utils/Mesh/Resource/MeshResource.h"
#include "utils/material/Material.h"
#include <cmath>
#include <cstdio>

using namespace DXEngine;
using namespace DirectX;

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
static bool Near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }

static XMFLOAT4X4 Matrix(const XMMATRIX& m) { XMFLOAT4X4 f; XMStoreFloat4x4(&f, m); return f; }

static std::shared_ptr<Mesh> CubeMesh() { return Mesh::CreateCube(nullptr, 1.0f); }

// A triangle whose vertices carry bone indices + weights: the layout a skinned mesh has.
static std::shared_ptr<Mesh> SkinnedTriangle()
{
    auto vertices = std::make_unique<VertexData>(VertexLayout::CreateSkinned());
    vertices->Resize(3);
    for (size_t i = 0; i < 3; ++i)
        vertices->SetAttribute(i, RHI::VertexAttributeType::Position, XMFLOAT3{ float(i), 0.0f, 0.0f });
    auto indices = std::make_unique<IndexData>(IndexType::UInt16);
    indices->AddTriangle(0, 1, 2);
    auto resource = std::make_shared<MeshResource>("SkinnedTriangle");
    resource->SetVertexData(std::move(vertices));
    resource->SetIndexData(std::move(indices));
    return std::make_shared<Mesh>(resource, nullptr);
}

// two bones: "root" and its child; one clip that slides the root 4 units along X in 2 seconds
static std::shared_ptr<Skeleton> TwoBoneSkeleton()
{
    auto skeleton = std::make_shared<Skeleton>();
    Bone root;  root.Name = "root";
    Bone child; child.Name = "child"; child.ParentIndex = 0;
    skeleton->AddBone(root);
    skeleton->AddBone(child);
    return skeleton;
}

static std::shared_ptr<AnimationClip> SlideClip(const char* name = "slide", float duration = 2.0f)
{
    auto clip = std::make_shared<AnimationClip>(name, duration);
    clip->SetTicksPerSecond(1.0f);                                     // ticks == seconds, to keep the numbers readable
    Keyframe a, b;
    a.TimeStamp = 0.0f;     a.Translation = { 0, 0, 0 };
    b.TimeStamp = duration; b.Translation = { 4, 0, 0 };
    clip->AddBoneAnimation("root", { a, b });
    return clip;
}

// ------------------------------------------------------------------ the asset
static void NodesKeepTheirLocalTransform()
{
    ModelAsset asset;
    const XMMATRIX local = XMMatrixScaling(2, 2, 2) * XMMatrixRotationY(XM_PIDIV2) * XMMatrixTranslation(1, 2, 3);
    const uint32_t n = asset.AddNode("n", -1, Matrix(local));
    const ModelNode& node = asset.nodes[n];
    CHECK(Near(node.position.x, 1) && Near(node.position.y, 2) && Near(node.position.z, 3));
    CHECK(Near(node.scale.x, 2) && Near(node.scale.y, 2) && Near(node.scale.z, 2));
    // the rotation: 90 degrees around Y
    const XMVECTOR q = XMLoadFloat4(&node.rotation);
    const XMVECTOR expected = XMQuaternionRotationAxis(XMVectorSet(0, 1, 0, 0), XM_PIDIV2);
    CHECK(Near(XMVectorGetX(XMVector4Dot(q, expected)), 1.0f) || Near(XMVectorGetX(XMVector4Dot(q, expected)), -1.0f));
    CHECK(asset.warnings.empty());

    XMFLOAT4X4 flat = Matrix(XMMatrixScaling(0, 0, 0));                // a zero scale is kept but reported: it hides everything below
    const uint32_t d = asset.AddNode("flat", int(n), flat);
    CHECK(Near(asset.nodes[d].scale.x, 0) && asset.warnings.size() == 1);

    XMFLOAT4X4 broken = Matrix(XMMatrixIdentity()); broken._11 = NAN;  // not even a matrix: identity and a warning, never garbage
    const uint32_t b = asset.AddNode("broken", int(n), broken);
    CHECK(std::isfinite(asset.nodes[b].position.x) && std::isfinite(asset.nodes[b].scale.x));
    CHECK(asset.warnings.size() >= 2);
}

static void ScaleLandsOnTheRoot()
{
    ModelAsset asset;
    ModelNode root; root.name = "r"; root.position = { 1, 0, 0 }; root.scale = { 2, 2, 2 };
    asset.AddNode(root);
    asset.ApplyScale(10.0f);
    CHECK(Near(asset.nodes[0].scale.x, 20) && Near(asset.nodes[0].position.x, 10));
    asset.ApplyScale(1.0f);
    CHECK(Near(asset.nodes[0].scale.x, 20));
}

static void ValidateNamesWhatIsWrong()
{
    ModelAsset empty;
    CHECK(empty.Validate().size() == 1);                               // no nodes

    ModelAsset ok;
    ok.AddNode("root", -1, Matrix(XMMatrixIdentity()));
    const uint32_t m = ok.AddMesh("cube", CubeMesh());
    ok.AttachMesh(0, m);
    CHECK(ok.Validate().empty());

    ModelAsset bad;
    bad.AddNode("root", -1, Matrix(XMMatrixIdentity()));
    bad.AddNode("late", 5, Matrix(XMMatrixIdentity()));                // parent after the child
    bad.AttachMesh(1, 7);                                              // no such mesh
    bad.AddMesh("null", nullptr);
    bad.clips.push_back(nullptr);
    const auto problems = bad.Validate();
    CHECK(problems.size() == 4);

    ModelAsset skinnedNoSkeleton;
    skinnedNoSkeleton.AddNode("root", -1, Matrix(XMMatrixIdentity()));
    skinnedNoSkeleton.AddMesh("s", SkinnedTriangle());
    CHECK(skinnedNoSkeleton.meshes[0].skinned);                        // read from the vertex layout
    CHECK(!skinnedNoSkeleton.IsSkinned() && skinnedNoSkeleton.Validate().size() == 1);
}

// ------------------------------------------------------------------ instantiating
static std::shared_ptr<ModelAsset> StaticAsset()
{
    // root
    //   +-- arm (moved 5 up)       -> one mesh: the node's entity carries it
    //        +-- hand (moved 2 right) -> two meshes: one child entity each
    auto asset = std::make_shared<ModelAsset>();
    asset->AddNode("RootNode", -1, Matrix(XMMatrixScaling(0.5f, 0.5f, 0.5f)));
    asset->AddNode("arm", 0, Matrix(XMMatrixTranslation(0, 5, 0)));
    asset->AddNode("hand", 1, Matrix(XMMatrixTranslation(2, 0, 0)));
    const uint32_t cube = asset->AddMesh("cube", CubeMesh());
    const uint32_t ball = asset->AddMesh("ball", Mesh::CreateSphere(nullptr, 0.5f, 8));
    asset->AttachMesh(1, cube);
    asset->AttachMesh(2, cube);                                        // the same mesh on two nodes
    asset->AttachMesh(2, ball);
    return asset;
}

static void InstantiateBuildsTheNodeTree()
{
    Scene scene;
    auto asset = StaticAsset();
    CHECK(asset->Validate().empty());
    Entity root = SceneFactory::Instantiate(scene, asset, "Statue");
    CHECK(root.IsValid() && root.Get<NameComponent>().name == "Statue");       // the root takes the instance's name
    CHECK(!root.Has<AnimatorComponent>());

    auto kids = scene.GetChildren(root);
    CHECK(kids.size() == 1 && kids[0].Get<NameComponent>().name == "arm");
    Entity arm = kids[0];
    CHECK(arm.Has<MeshRendererComponent>());                           // one mesh on the node: the node itself shows it
    CHECK(arm.Get<MeshRendererComponent>().mesh == asset->meshes[0].mesh);   // shared with the asset, not copied

    auto armKids = scene.GetChildren(arm);
    CHECK(armKids.size() == 1 && armKids[0].Get<NameComponent>().name == "hand");
    Entity hand = armKids[0];
    CHECK(!hand.Has<MeshRendererComponent>());                         // two meshes on the node: one child entity each
    auto parts = scene.GetChildren(hand);
    CHECK(parts.size() == 2);
    for (Entity part : parts)                                          // (the order of children is not part of the contract)
    {
        const std::string& pn = part.Get<NameComponent>().name;
        CHECK(pn == "cube" || pn == "ball");
        CHECK(part.Get<MeshRendererComponent>().mesh == (pn == "cube" ? asset->meshes[0].mesh : asset->meshes[1].mesh));   // shared, not copied
    }

    // the transforms compose like the node tree says: root scale 0.5, arm up 5, hand right 2
    TransformSystem::Update(scene);
    const XMFLOAT4X4 w = hand.Get<TransformComponent>().world;
    XMVECTOR s, r, t;
    XMMatrixDecompose(&s, &r, &t, XMLoadFloat4x4(&w));
    CHECK(Near(XMVectorGetX(t), 1.0f) && Near(XMVectorGetY(t), 2.5f));          // (2 right, 5 up) at half scale
    CHECK(Near(XMVectorGetX(s), 0.5f));
}

static void TwoInstancesShareTheDataNotTheEntities()
{
    Scene scene;
    auto asset = StaticAsset();
    Entity a = SceneFactory::Instantiate(scene, asset, "A");
    Entity b = SceneFactory::Instantiate(scene, asset, "B");
    CHECK(a != b);
    Entity armA = scene.GetChildren(a)[0], armB = scene.GetChildren(b)[0];
    CHECK(armA != armB);
    CHECK(armA.Get<MeshRendererComponent>().mesh == armB.Get<MeshRendererComponent>().mesh);
    armA.Get<TransformComponent>().position.y = 100.0f;                // moving one instance does not move the other
    CHECK(Near(armB.Get<TransformComponent>().position.y, 5.0f));
    CHECK(!SceneFactory::Instantiate(scene, nullptr, "x").IsValid());
    CHECK(!SceneFactory::Instantiate(scene, std::make_shared<ModelAsset>(), "x").IsValid());
}

static std::shared_ptr<ModelAsset> SkinnedAsset()
{
    auto asset = std::make_shared<ModelAsset>();
    asset->AddNode("Armature", -1, Matrix(XMMatrixIdentity()));
    asset->AddNode("Body", 0, Matrix(XMMatrixTranslation(0, 9, 0)));   // a node transform a skinned mesh must NOT inherit
    asset->AddNode("Prop", 0, Matrix(XMMatrixTranslation(1, 0, 0)));
    asset->skeleton = TwoBoneSkeleton();
    asset->clips = { SlideClip("slide", 2.0f), SlideClip("walk", 1.0f) };
    const uint32_t body = asset->AddMesh("body", SkinnedTriangle());
    const uint32_t prop = asset->AddMesh("prop", CubeMesh());          // a static mesh in a skinned model
    asset->AttachMesh(1, body);
    asset->AttachMesh(2, prop);
    return asset;
}

static void SkinnedInstantiateGetsAnAnimator()
{
    Scene scene;
    auto asset = SkinnedAsset();
    CHECK(asset->Validate().empty() && asset->IsSkinned());
    Entity root = SceneFactory::Instantiate(scene, asset, "Hero");
    CHECK(root.Has<AnimatorComponent>());
    const AnimatorComponent& a = root.Get<AnimatorComponent>();
    CHECK(a.skeleton == asset->skeleton && a.clips.size() == 2 && a.clip == 0 && a.playing);

    // the skinned mesh hangs on the root with no transform of its own; the static one follows its node
    Entity body, prop;
    for (Entity c : scene.GetChildren(root))
    {
        const std::string& n = c.Get<NameComponent>().name;
        if (n == "body") body = c;
        if (n == "Prop") prop = c;
    }
    CHECK(body.IsValid() && body.Has<SkinnedMeshComponent>() && body.Has<MeshRendererComponent>());
    CHECK(Near(body.Get<TransformComponent>().position.y, 0.0f));
    CHECK(prop.IsValid() && !prop.Has<SkinnedMeshComponent>() && Near(prop.Get<TransformComponent>().position.x, 1.0f));

    // the mesh finds the Animator above it, wherever it hangs
    CHECK(AnimationSystem::FindAnimator(scene, body.Id()) == &root.Get<AnimatorComponent>());
    CHECK(AnimationSystem::FindAnimator(scene, prop.Id()) == &root.Get<AnimatorComponent>());
    Entity loose = scene.CreateEntity("loose");
    CHECK(AnimationSystem::FindAnimator(scene, loose.Id()) == nullptr);
    CHECK(AnimationSystem::FindAnimator(scene, entt::null) == nullptr);
}

// ------------------------------------------------------------------ the animator
static Entity Animated(Scene& scene, std::shared_ptr<ModelAsset> asset = nullptr)
{
    return SceneFactory::Instantiate(scene, asset ? asset : SkinnedAsset(), "Hero");
}

static void TheSystemAdvancesAndPublishesBones()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimatorComponent& a = hero.Get<AnimatorComponent>();
    CHECK(a.boneMatrices.empty());                                     // nothing before the first update

    AnimationSystem::Update(scene, 0.5f);
    CHECK(Near(a.time, 0.5f) && a.playing);
    CHECK(a.boneMatrices.size() == 2);                                 // one matrix per bone
    CHECK(Near(a.boneMatrices[0]._14, 1.0f));                          // 0.5 s of a 4-unit-in-2-s slide: 1 unit

    AnimationSystem::Update(scene, 0.5f);
    CHECK(Near(a.time, 1.0f) && Near(a.boneMatrices[0]._14, 2.0f));
}

static void SettingsAreAppliedEveryFrame()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimatorComponent& a = hero.Get<AnimatorComponent>();

    a.speed = 2.0f;
    AnimationSystem::Update(scene, 0.25f);
    CHECK(Near(a.time, 0.5f));                                         // twice as fast

    a.speed = -1.0f;                                                   // backwards
    AnimationSystem::Update(scene, 0.25f);
    CHECK(Near(a.time, 0.25f));

    AnimationSystem::Pause(a);
    AnimationSystem::Update(scene, 1.0f);
    CHECK(Near(a.time, 0.25f) && !a.playing);                          // paused: time does not move, bones still published
    CHECK(a.boneMatrices.size() == 2);

    AnimationSystem::Play(a);
    a.speed = 1.0f;
    AnimationSystem::Update(scene, 0.25f);
    CHECK(Near(a.time, 0.5f) && a.playing);

    AnimationSystem::Stop(a);
    AnimationSystem::Update(scene, 1.0f);
    CHECK(Near(a.time, 0.0f) && !a.playing);
}

static void ScrubbingJumpsToTheTime()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimatorComponent& a = hero.Get<AnimatorComponent>();
    AnimationSystem::Update(scene, 0.1f);

    AnimationSystem::Pause(a);
    a.time = 1.5f;                                                     // what the inspector's slider writes
    AnimationSystem::Update(scene, 0.0f);
    CHECK(Near(a.time, 1.5f) && Near(a.boneMatrices[0]._14, 3.0f));    // the pose follows

    a.time = 99.0f;                                                    // beyond the clip: clamped, never extrapolated
    AnimationSystem::Update(scene, 0.0f);
    CHECK(Near(a.time, 2.0f) && Near(a.boneMatrices[0]._14, 4.0f));
}

static void SwitchingClipsRestartsAndClamps()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimatorComponent& a = hero.Get<AnimatorComponent>();
    AnimationSystem::Update(scene, 1.5f);
    CHECK(Near(a.time, 1.5f));

    AnimationSystem::SetClip(a, 1);                                    // "walk", 1 s long
    AnimationSystem::Update(scene, 0.25f);
    CHECK(a.clip == 1 && Near(a.time, 0.25f));                         // restarted at 0, then stepped

    AnimationSystem::SetClip(a, 99);
    CHECK(a.clip == 1);                                                // clamped to the last clip
    AnimationSystem::SetClip(a, -5);
    CHECK(a.clip == 0);

    AnimatorComponent none;
    AnimationSystem::SetClip(none, 3);
    CHECK(none.clip == -1);                                            // no clips: nothing to select
}

static void ModesBehaveAsNamed()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimatorComponent& a = hero.Get<AnimatorComponent>();

    a.mode = PlaybackMode::Once;
    AnimationSystem::Update(scene, 3.0f);                              // past the end of a 2 s clip
    CHECK(Near(a.time, 2.0f) && !a.playing);                           // stopped at the end by itself
    AnimationSystem::Play(a);                                          // Play after the end starts over
    CHECK(Near(a.time, 0.0f) && a.playing);

    a.mode = PlaybackMode::Loop;
    a.time = 0.0f;
    AnimationSystem::Update(scene, 0.0f);
    AnimationSystem::Update(scene, 2.5f);
    CHECK(Near(a.time, 0.5f) && a.playing);                            // wrapped

    a.mode = PlaybackMode::PingPong;
    a.time = 1.5f;
    AnimationSystem::Update(scene, 0.0f);
    AnimationSystem::Update(scene, 1.0f);                              // reaches the end and turns around
    CHECK(Near(a.time, 2.0f));
    AnimationSystem::Update(scene, 0.5f);                              // and now runs backwards
    CHECK(Near(a.time, 1.5f));
}

static void ADegenerateClipIsHarmless()
{
    Scene scene;
    auto asset = SkinnedAsset();
    asset->clips = { SlideClip("empty", 0.0f) };                       // zero length
    Entity hero = SceneFactory::Instantiate(scene, asset, "Hero");
    AnimatorComponent& a = hero.Get<AnimatorComponent>();
    for (int i = 0; i < 3; ++i) AnimationSystem::Update(scene, 0.5f);
    CHECK(std::isfinite(a.time) && Near(a.time, 0.0f));                // (fmod by zero used to make this NaN)
    for (const XMFLOAT4X4& m : a.boneMatrices) CHECK(std::isfinite(m._14) && std::isfinite(m._11));

    asset = SkinnedAsset();
    asset->clips.clear();                                              // a skeleton and no clips: bind pose, no bones published
    Entity still = SceneFactory::Instantiate(scene, asset, "Still");
    CHECK(still.Get<AnimatorComponent>().clip == -1 && !still.Get<AnimatorComponent>().playing);
    AnimationSystem::Update(scene, 1.0f);
    CHECK(still.Get<AnimatorComponent>().boneMatrices.empty());
}

// A clip from a model file is in TICKS (60 ticks at 30 per second = 2 seconds). The controller plays in seconds: the
// clip must last 2 seconds, not 60, and its pose at 1 s is halfway.
static void ClipsFromFilesPlayInSecondsNotTicks()
{
    auto clip = std::make_shared<AnimationClip>("file", 60.0f);        // default 30 ticks per second
    CHECK(Near(clip->GetDurationSeconds(), 2.0f));
    Keyframe a, b; a.TimeStamp = 0.0f; b.TimeStamp = 60.0f; b.Translation = { 4, 0, 0 };
    clip->AddBoneAnimation("root", { a, b });

    Scene scene;
    auto asset = SkinnedAsset();
    asset->clips = { clip };
    Entity hero = SceneFactory::Instantiate(scene, asset, "Hero");
    AnimatorComponent& an = hero.Get<AnimatorComponent>();
    an.mode = PlaybackMode::Once;

    AnimationSystem::Update(scene, 1.0f);
    CHECK(Near(an.time, 1.0f) && Near(an.boneMatrices[0]._14, 2.0f));  // halfway after 1 of 2 seconds
    AnimationSystem::Update(scene, 5.0f);
    CHECK(Near(an.time, 2.0f) && !an.playing);                         // the end: after 2 seconds, not 60
    CHECK(Near(an.boneMatrices[0]._14, 4.0f));                         // and it shows the LAST pose, not a wrapped first one

    CHECK(Near(AnimationClip("none", 0.0f).GetDurationSeconds(), 0.0f));
    AnimationClip noRate("x", 10.0f); noRate.SetTicksPerSecond(0.0f);
    CHECK(Near(noRate.GetDurationSeconds(), 0.0f));                    // no rate: no length, never a division by zero
}

static void ACopiedAnimatorHasItsOwnPlayback()
{
    Scene scene;
    Entity hero = Animated(scene);
    AnimationSystem::Update(scene, 0.5f);

    // what Duplicate does: copy the component. The copy must not share the controller.
    Entity twin = scene.CreateEntity("Twin");
    twin.Add<AnimatorComponent>(hero.Get<AnimatorComponent>());
    AnimatorComponent& a = hero.Get<AnimatorComponent>();
    AnimatorComponent& b = twin.Get<AnimatorComponent>();
    CHECK(a.runtime.controller && !b.runtime.controller);

    b.speed = 3.0f;
    AnimationSystem::Update(scene, 0.25f);
    CHECK(Near(a.time, 0.75f));                                        // the original advanced once, at its own speed
    CHECK(Near(b.time, 0.5f + 0.75f));                                 // the copy continued from the copied time at ITS speed (same pose at the copy)
    CHECK(a.runtime.controller != b.runtime.controller);
}

int main()
{
    NodesKeepTheirLocalTransform();
    ScaleLandsOnTheRoot();
    ValidateNamesWhatIsWrong();
    InstantiateBuildsTheNodeTree();
    TwoInstancesShareTheDataNotTheEntities();
    SkinnedInstantiateGetsAnAnimator();
    TheSystemAdvancesAndPublishesBones();
    SettingsAreAppliedEveryFrame();
    ScrubbingJumpsToTheTime();
    SwitchingClipsRestartsAndClamps();
    ModesBehaveAsNamed();
    ADegenerateClipIsHarmless();
    ClipsFromFilesPlayInSecondsNotTicks();
    ACopiedAnimatorHasItsOwnPlayback();
    if (g_failures) { std::printf("%d check(s) failed\n", g_failures); return 1; }
    std::printf("All asset tests passed\n");
    return 0;
}
