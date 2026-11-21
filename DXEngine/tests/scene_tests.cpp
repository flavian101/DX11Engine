// Scene core tests. No GPU, no window: plain asserts so they run anywhere.
//   build: see DXEngine/tests/CMakeLists.txt  (cmake -DDXENGINE_BUILD_TESTS=ON)
#include "scene/Scene.h"
#include "scene/TransformSystem.h"
#include <algorithm>
#include <chrono>
#include <functional>
#include <cmath>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <unordered_map>

using namespace DXEngine;
using namespace DirectX;

static int g_failed = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("  FAILED line %d: %s\n", __LINE__, #cond); ++g_failed; } } while (0)
static bool Near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

static void TestWorldFromLocal()
{
    Scene scene;
    Entity e = scene.CreateEntity("box");
    auto& t = e.Get<TransformComponent>();
    t.position = { 1, 2, 3 }; t.scale = { 2, 2, 2 };
    TransformSystem::Update(scene);
    CHECK(Near(t.WorldPosition().x, 1) && Near(t.WorldPosition().y, 2) && Near(t.WorldPosition().z, 3));
    // a point at local (1,0,0) ends up at (1 + 2*1, 2, 3)
    XMFLOAT3 p; XMStoreFloat3(&p, XMVector3Transform(XMVectorSet(1, 0, 0, 1), t.World()));
    CHECK(Near(p.x, 3) && Near(p.y, 2) && Near(p.z, 3));
}

static void TestParentChildComposition()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child  = scene.CreateEntity("child", parent);
    parent.Get<TransformComponent>().position = { 10, 0, 0 };
    parent.Get<TransformComponent>().scale = { 2, 2, 2 };
    child.Get<TransformComponent>().position = { 1, 0, 0 };
    TransformSystem::Update(scene);
    // child local (1,0,0) is scaled by 2 and moved by the parent: world x = 10 + 2 = 12
    CHECK(Near(child.Get<TransformComponent>().WorldPosition().x, 12));
}

static void TestParentRotationAffectsChild()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child  = scene.CreateEntity("child", parent);
    parent.Get<TransformComponent>().SetEulerDegrees(0, 90, 0);          // yaw 90deg: +Z -> +X
    child.Get<TransformComponent>().position = { 0, 0, 5 };              // 5 units in front of the parent
    TransformSystem::Update(scene);
    auto w = child.Get<TransformComponent>().WorldPosition();
    CHECK(Near(w.x, 5) && Near(w.y, 0) && Near(w.z, 0));
    auto f = parent.Get<TransformComponent>().WorldForward();
    CHECK(Near(f.x, 1) && Near(f.z, 0));
}

static void TestDeepHierarchyCreatedOutOfOrder()
{
    Scene scene;
    // create the grandchild FIRST in storage order, then wire the chain: ordering must not depend on creation order
    Entity c = scene.CreateEntity("c"); Entity b = scene.CreateEntity("b"); Entity a = scene.CreateEntity("a");
    CHECK(scene.SetParent(c, b)); CHECK(scene.SetParent(b, a));
    a.Get<TransformComponent>().position = { 1, 0, 0 };
    b.Get<TransformComponent>().position = { 10, 0, 0 };
    c.Get<TransformComponent>().position = { 100, 0, 0 };
    TransformSystem::Update(scene);
    CHECK(Near(c.Get<TransformComponent>().WorldPosition().x, 111));
}

static void TestCyclesAreRejected()
{
    Scene scene;
    Entity a = scene.CreateEntity("a"), b = scene.CreateEntity("b"), c = scene.CreateEntity("c");
    CHECK(scene.SetParent(b, a)); CHECK(scene.SetParent(c, b));
    CHECK(!scene.SetParent(a, c));          // a under its own grandchild
    CHECK(!scene.SetParent(a, a));          // itself
    CHECK(!scene.GetParent(a));             // unchanged: still a root
    CHECK(scene.GetParent(c) == b);
    CHECK(scene.SetParent(c, Entity()));    // unparent
    CHECK(!scene.GetParent(c));
}

static void TestDestroyRemovesDescendantsAndStaleHandlesAreSafe()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    Entity child = scene.CreateEntity("child", parent);
    Entity grand = scene.CreateEntity("grand", child);
    Entity other = scene.CreateEntity("other");
    CHECK(scene.EntityCount() == 4);

    scene.DestroyEntity(parent);
    CHECK(scene.EntityCount() == 1);
    CHECK(!parent && !child && !grand);                         // all stale
    CHECK(other.IsValid());
    CHECK(child.TryGet<TransformComponent>() == nullptr);       // no crash, just null
    CHECK(!child.Has<TransformComponent>());

    bool threw = false;
    try { child.Get<TransformComponent>(); } catch (const std::runtime_error&) { threw = true; }
    CHECK(threw);                                               // a clear error instead of reading freed memory

    // a NEW entity may reuse the old slot; the old handle must NOT alias it
    Entity fresh = scene.CreateEntity("fresh");
    CHECK(fresh.IsValid());
    CHECK(!parent.IsValid());
}

static void TestComponentsAddReplaceRemove()
{
    Scene scene;
    Entity e = scene.CreateEntity("lamp");
    CHECK(!e.Has<LightComponent>());
    e.Add<LightComponent>(LightComponent{ LightType::Spot, { 1, 0, 0 }, 5.0f, 20.0f, 10.0f, 25.0f, true });
    CHECK(e.Has<LightComponent>() && e.Get<LightComponent>().type == LightType::Spot);
    e.Add<LightComponent>().intensity = 9.0f;                    // replaces with a default-constructed one
    CHECK(e.Get<LightComponent>().type == LightType::Point && Near(e.Get<LightComponent>().intensity, 9.0f));
    e.Remove<LightComponent>();
    CHECK(!e.Has<LightComponent>());
}

static void TestFindAndChildren()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", root);
    CHECK(scene.FindByName("b") == b);
    CHECK(!scene.FindByName("missing"));
    CHECK(scene.GetChildren(root).size() == 2);
    CHECK(scene.GetChildren(a).empty());
}

static void TestManyEntitiesUnderOneParent()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    root.Get<TransformComponent>().position = { 0, 100, 0 };
    for (int i = 0; i < 5000; ++i)
        scene.CreateEntity("n", root).Get<TransformComponent>().position = { float(i), 0, 0 };
    TransformSystem::Update(scene);
    size_t ok = 0;
    for (auto [e, t] : scene.Registry().view<TransformComponent>().each())
        if (e != root.Id() && Near(t.WorldPosition().y, 100)) ++ok;
    CHECK(ok == 5000);
}

static void TestDestroyLargeSubtree()
{
    Scene scene;
    Entity bystander = scene.CreateEntity("bystander");
    Entity root = scene.CreateEntity("root");
    Entity keptChild;                                      // a sibling subtree that must survive
    for (int i = 0; i < 2000; ++i)
    {
        Entity child = scene.CreateEntity("c", root);
        for (int g = 0; g < 3; ++g) scene.CreateEntity("g", child);
    }
    Entity other = scene.CreateEntity("other");
    keptChild = scene.CreateEntity("kept", other);
    CHECK(scene.EntityCount() == 1 + 1 + 2000 * 4 + 1 + 1);

    scene.DestroyEntity(root);                             // 8001 entities: must be linear, not quadratic
    CHECK(!root.IsValid());
    CHECK(scene.EntityCount() == 3);                       // bystander, other, kept
    CHECK(bystander.IsValid() && other.IsValid() && keptChild.IsValid());
    CHECK(scene.GetParent(keptChild) == other);

    scene.DestroyEntity(other);                            // a subtree that is not at the top of the registry
    CHECK(!keptChild.IsValid() && bystander.IsValid() && scene.EntityCount() == 1);
}

// ---------------------------------------------------------------------------------------------------------------------
// Hierarchy links (RelationshipComponent). Scene is the only writer; these tests read the raw links to prove it keeps them
// consistent, because everything above it (children, destroy, transforms) trusts them.
// ---------------------------------------------------------------------------------------------------------------------
static const RelationshipComponent* Rel(Scene& scene, entt::entity e)
{
    return scene.Registry().valid(e) ? scene.Registry().try_get<RelationshipComponent>(e) : nullptr;
}

// Every component must describe a real list: forward links, back links and the ends agree, no leftover empty components.
static bool HierarchyConsistent(Scene& scene)
{
    entt::registry& reg = scene.Registry();
    for (auto [e, r] : reg.view<RelationshipComponent>().each())
    {
        if (r.parent == entt::null && r.firstChild == entt::null) return false;           // should have been removed
        if ((r.firstChild == entt::null) != (r.lastChild == entt::null)) return false;

        entt::entity prev = entt::null;
        size_t count = 0;
        for (entt::entity c = r.firstChild; c != entt::null;)
        {
            const RelationshipComponent* cr = Rel(scene, c);
            if (!cr || cr->parent != e || cr->prevSibling != prev) return false;
            prev = c;
            c = cr->nextSibling;
            if (++count > 100000) return false;                                              // a loop
        }
        if (r.lastChild != prev) return false;

        if (r.parent != entt::null)
        {
            const RelationshipComponent* pr = Rel(scene, r.parent);
            if (!pr) return false;
            if (r.prevSibling == entt::null ? pr->firstChild != e : Rel(scene, r.prevSibling)->nextSibling != e) return false;
            if (r.nextSibling == entt::null ? pr->lastChild  != e : Rel(scene, r.nextSibling)->prevSibling != e) return false;
        }
        else if (r.prevSibling != entt::null || r.nextSibling != entt::null) return false;
    }
    return true;
}

static std::vector<std::string> NamesOf(Scene& scene, Entity parent)
{
    std::vector<std::string> names;
    for (Entity c : scene.GetChildren(parent)) names.push_back(c.Get<NameComponent>().name);
    return names;
}
using Names = std::vector<std::string>;

static void TestChildrenAreInAttachOrder()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", root), c = scene.CreateEntity("c", root);
    CHECK(NamesOf(scene, root) == (Names{ "a", "b", "c" }));
    CHECK(scene.FirstChild(root) == a && scene.NextSibling(a) == b && scene.NextSibling(b) == c && !scene.NextSibling(c));
    CHECK(!scene.FirstChild(a) && !scene.NextSibling(root));
    CHECK(scene.GetChildren(Entity()).empty() && !scene.FirstChild(Entity()));
    CHECK(HierarchyConsistent(scene));
}

static void TestDetachFirstMiddleLast()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", root), c = scene.CreateEntity("c", root), d = scene.CreateEntity("d", root);

    CHECK(scene.SetParent(b, Entity()));                       // middle
    CHECK(NamesOf(scene, root) == (Names{ "a", "c", "d" }) && HierarchyConsistent(scene));
    CHECK(!scene.GetParent(b) && !b.Has<RelationshipComponent>());   // a lone root carries no links

    CHECK(scene.SetParent(a, Entity()));                       // first
    CHECK(NamesOf(scene, root) == (Names{ "c", "d" }) && HierarchyConsistent(scene));

    CHECK(scene.SetParent(d, Entity()));                       // last
    CHECK(NamesOf(scene, root) == (Names{ "c" }) && HierarchyConsistent(scene));

    CHECK(scene.SetParent(c, Entity()));                       // the only one: the parent has no children left
    CHECK(scene.GetChildren(root).empty() && !root.Has<RelationshipComponent>() && !c.Has<RelationshipComponent>());
    CHECK(HierarchyConsistent(scene));

    CHECK(scene.SetParent(c, Entity()));                       // unparenting a root is a no-op that succeeds
}

static void TestReparent()
{
    Scene scene;
    Entity p = scene.CreateEntity("p"), q = scene.CreateEntity("q");
    Entity a = scene.CreateEntity("a", p), b = scene.CreateEntity("b", p), x = scene.CreateEntity("x", q);

    CHECK(scene.SetParent(a, p));                              // the parent it already has: the order must not change
    CHECK(NamesOf(scene, p) == (Names{ "a", "b" }));

    CHECK(scene.SetParent(a, q));                              // moves to the END of the new parent
    CHECK(NamesOf(scene, p) == (Names{ "b" }) && NamesOf(scene, q) == (Names{ "x", "a" }));
    CHECK(scene.GetParent(a) == q && HierarchyConsistent(scene));

    CHECK(scene.SetParent(b, x));                              // an entity that has children can move too: they go with it
    CHECK(scene.SetParent(q, p));
    CHECK(scene.GetParent(b) == x && scene.GetParent(x) == q && scene.GetParent(q) == p);
    CHECK(p.Has<RelationshipComponent>());                     // p still has a child (q)
    CHECK(HierarchyConsistent(scene));

    CHECK(!scene.SetParent(p, b));                             // a cycle through the new links is still refused,
    CHECK(!scene.GetParent(p) && NamesOf(scene, p) == (Names{ "q" }) && HierarchyConsistent(scene));   // and changes nothing
}

static void TestDestroyUnlinksFromParent()
{
    Scene scene;
    Entity root = scene.CreateEntity("root");
    Entity a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", root), c = scene.CreateEntity("c", root);
    Entity bKid = scene.CreateEntity("bKid", b), bGrandKid = scene.CreateEntity("bGrandKid", bKid);

    scene.DestroyEntity(b);                                    // the middle child, with a subtree
    CHECK(!b.IsValid() && !bKid.IsValid() && !bGrandKid.IsValid());
    CHECK(NamesOf(scene, root) == (Names{ "a", "c" }) && HierarchyConsistent(scene));

    scene.DestroyEntity(a);                                    // the first
    CHECK(NamesOf(scene, root) == (Names{ "c" }) && HierarchyConsistent(scene));

    scene.DestroyEntity(c);                                    // the last one: root has nothing left
    CHECK(scene.GetChildren(root).empty() && !root.Has<RelationshipComponent>() && HierarchyConsistent(scene));
    CHECK(scene.EntityCount() == 1);

    Entity d = scene.CreateEntity("d", root);                  // and the parent is usable again
    CHECK(NamesOf(scene, root) == (Names{ "d" }) && HierarchyConsistent(scene));
    (void)d;
}

// A random sequence of edits against a plain reference model (parent map + ordered child vectors). After EVERY edit the
// scene must answer exactly as the model does and its raw links must be consistent.
static void TestHierarchyAgainstReferenceModel()
{
    Scene scene;
    std::mt19937 rng(12345);
    const auto pick = [&](size_t n) { return static_cast<size_t>(rng() % n); };

    std::vector<Entity> alive;
    std::unordered_map<entt::entity, entt::entity> parentOf;                    // absent = root
    std::unordered_map<entt::entity, std::vector<entt::entity>> kidsOf;

    const auto removeFromParent = [&](entt::entity e)
    {
        const auto it = parentOf.find(e);
        if (it == parentOf.end()) return;
        auto& list = kidsOf[it->second];
        list.erase(std::find(list.begin(), list.end(), e));
        parentOf.erase(it);
    };
    const auto isAncestorOrSelf = [&](entt::entity maybeAncestor, entt::entity e)
    {
        for (; e != entt::null; )
        {
            if (e == maybeAncestor) return true;
            const auto it = parentOf.find(e);
            e = it == parentOf.end() ? entt::null : it->second;
        }
        return false;
    };

    int refused = 0, moved = 0, destroyed = 0;
    for (int step = 0; step < 4000; ++step)
    {
        const size_t op = pick(10);
        if (alive.empty() || op < 3 || alive.size() < 4)                               // create (maybe under a parent)
        {
            Entity parent = (!alive.empty() && pick(3) != 0) ? alive[pick(alive.size())] : Entity();
            Entity e = scene.CreateEntity("e", parent);
            alive.push_back(e);
            if (parent.IsValid()) { parentOf[e.Id()] = parent.Id(); kidsOf[parent.Id()].push_back(e.Id()); }
        }
        else if (op < 8)                                                               // SetParent (or unparent)
        {
            Entity child = alive[pick(alive.size())];
            Entity parent = pick(5) == 0 ? Entity() : alive[pick(alive.size())];
            const bool cycle = parent.IsValid() && isAncestorOrSelf(child.Id(), parent.Id());
            const bool ok = scene.SetParent(child, parent);
            CHECK(ok == !cycle);
            if (cycle) { ++refused; }
            else
            {
                const auto cur = parentOf.find(child.Id());
                const bool same = parent.IsValid() && cur != parentOf.end() && cur->second == parent.Id();
                if (!same)
                {
                    removeFromParent(child.Id());
                    if (parent.IsValid()) { parentOf[child.Id()] = parent.Id(); kidsOf[parent.Id()].push_back(child.Id()); }
                    ++moved;
                }
            }
        }
        else                                                                           // destroy a subtree
        {
            const size_t index = pick(alive.size());
            Entity victim = alive[index];
            std::vector<entt::entity> subtree{ victim.Id() };
            for (size_t i = 0; i < subtree.size(); ++i)
                for (entt::entity k : kidsOf[subtree[i]]) subtree.push_back(k);
            removeFromParent(victim.Id());
            scene.DestroyEntity(victim);
            for (entt::entity gone : subtree) { kidsOf.erase(gone); parentOf.erase(gone); }
            alive.erase(std::remove_if(alive.begin(), alive.end(), [&](const Entity& e) { return !e.IsValid(); }), alive.end());
            destroyed += static_cast<int>(subtree.size());
            CHECK(!victim.IsValid());
        }

        // the scene must agree with the model about everything
        bool agree = scene.EntityCount() == alive.size() && HierarchyConsistent(scene);
        for (const Entity& e : alive)
        {
            const auto it = parentOf.find(e.Id());
            const Entity parent = scene.GetParent(e);
            agree = agree && (it == parentOf.end() ? !parent.IsValid() : parent.Id() == it->second);
            const auto& expected = kidsOf[e.Id()];
            const std::vector<Entity> actual = scene.GetChildren(e);
            agree = agree && actual.size() == expected.size();
            for (size_t i = 0; agree && i < actual.size(); ++i) agree = actual[i].Id() == expected[i];
        }
        if (!agree) { CHECK(agree); std::printf("  (diverged at step %d)\n", step); return; }
    }
    CHECK(refused > 50 && moved > 500 && destroyed > 200);                            // the run really exercised all three
}

// "The children of X" must cost the children of X. With 200 000 other entities in the scene, 5000 calls would scan a billion
// entities if GetChildren scanned (the old ParentComponent version did); with the links they touch three each.
static void TestChildLookupDoesNotScanTheScene()
{
    Scene scene;
    Entity parent = scene.CreateEntity("parent");
    for (int i = 0; i < 3; ++i) scene.CreateEntity("kid", parent);
    for (int i = 0; i < 200000; ++i) scene.CreateEntity("other");
    Entity last = scene.CreateEntity("last", parent);

    const auto start = std::chrono::steady_clock::now();
    size_t total = 0;
    for (int i = 0; i < 5000; ++i) total += scene.GetChildren(parent).size();
    scene.DestroyEntity(last);
    const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();

    CHECK(total == 5000 * 4);
    CHECK(scene.GetChildren(parent).size() == 3);
    CHECK(seconds < 0.5);
    if (seconds >= 0.5) std::printf("  (took %.3f s)\n", seconds);
}

// ---------------------------------------------------------------------------------------------------------------------
// TransformSystem: only what changed is recomputed, and the result is the same as computing everything from scratch.
// ---------------------------------------------------------------------------------------------------------------------
static XMMATRIX ReferenceWorld(Scene& scene, Entity e)                      // from scratch: local * parent's, all the way up
{
    XMMATRIX w = e.Get<TransformComponent>().Local();
    Entity parent = scene.GetParent(e);
    if (parent.IsValid() && parent.Has<TransformComponent>()) w = w * ReferenceWorld(scene, parent);
    return w;
}

static bool WorldMatches(Entity e, FXMMATRIX expected)
{
    XMFLOAT4X4 a, b;
    XMStoreFloat4x4(&a, e.Get<TransformComponent>().World());
    XMStoreFloat4x4(&b, expected);
    const float* pa = &a._11; const float* pb = &b._11;
    for (int i = 0; i < 16; ++i) if (!Near(pa[i], pb[i], 1e-3f)) return false;
    return true;
}

static void TestTransformRecomputesOnlyWhatChanged()
{
    Scene scene;
    Entity root = scene.CreateEntity("root"), a = scene.CreateEntity("a", root), b = scene.CreateEntity("b", a);
    Entity c = scene.CreateEntity("c", root), other = scene.CreateEntity("other");
    const auto move = [](Entity e, float x) { e.Get<TransformComponent>().position.x = x; };

    CHECK(TransformSystem::Update(scene) == 5);            // new entities: all of them
    CHECK(TransformSystem::Update(scene) == 0);            // nothing changed: nothing computed

    move(b, 1);                                            CHECK(TransformSystem::Update(scene) == 1);   // a leaf
    move(c, 2);                                            CHECK(TransformSystem::Update(scene) == 1);
    move(other, 3);                                        CHECK(TransformSystem::Update(scene) == 1);
    move(a, 4);                                            CHECK(TransformSystem::Update(scene) == 2);   // a and b below it
    move(root, 5);                                         CHECK(TransformSystem::Update(scene) == 4);   // the whole tree, not 'other'
    move(root, 6); move(a, 7); move(b, 8);                 CHECK(TransformSystem::Update(scene) == 4);   // changed at several depths: each once
    CHECK(Near(b.Get<TransformComponent>().WorldPosition().x, 6 + 7 + 8));

    CHECK(scene.SetParent(c, a));                          CHECK(TransformSystem::Update(scene) == 1);   // a new parent, no field changed
    CHECK(Near(c.Get<TransformComponent>().WorldPosition().x, 2 + 7 + 6));
    CHECK(scene.SetParent(a, other));                      CHECK(TransformSystem::Update(scene) == 3);   // a, b, c (c is under a now)
    CHECK(Near(b.Get<TransformComponent>().WorldPosition().x, 3 + 7 + 8));
    CHECK(scene.SetParent(c, c.GetScene()->GetParent(c)));  CHECK(TransformSystem::Update(scene) == 0);  // same parent: nothing to do
    CHECK(scene.SetParent(a, Entity()));                   CHECK(TransformSystem::Update(scene) == 3);   // back to a root
    CHECK(Near(b.Get<TransformComponent>().WorldPosition().x, 7 + 8));

    // A copied transform brings someone else's 'world' along: it must not be trusted.
    Entity d = scene.CreateEntity("d", other);
    CHECK(TransformSystem::Update(scene) == 1);
    d.Get<TransformComponent>() = root.Get<TransformComponent>();                 // what Duplicate does
    CHECK(TransformSystem::Update(scene) == 1);
    CHECK(WorldMatches(d, ReferenceWorld(scene, d)) && Near(d.Get<TransformComponent>().WorldPosition().x, 3 + 6));

    root.Get<TransformComponent>().MarkDirty();            // an explicit request recomputes without a field change
    CHECK(TransformSystem::Update(scene) == 1);            // root has no children left (a and c moved away): just itself
    CHECK(TransformSystem::Update(scene) == 0);
}

// Random edits (fields, reparenting, creation, destruction, copied transforms, MarkDirty). After every Update each world
// matrix must equal the from-scratch result, and the number of matrices written must be EXACTLY the set that needed it:
// the changed entities plus everything below them. Not fewer (stale), not more (wasted work).
static void TestTransformUpdateAgainstReference()
{
    Scene scene;
    std::mt19937 rng(777);
    const auto pick = [&](size_t n) { return static_cast<size_t>(rng() % n); };
    const auto number = [&]() { return static_cast<float>(static_cast<int>(rng() % 2001) - 1000) / 100.0f; };

    std::vector<Entity> alive;
    int updates = 0, mismatched = 0, wrongCount = 0;
    size_t totalRecomputed = 0, totalEntities = 0;

    for (int step = 0; step < 3000; ++step)
    {
        const size_t op = pick(12);
        if (alive.size() < 6 || op < 2)
        {
            Entity parent = (!alive.empty() && pick(4) != 0) ? alive[pick(alive.size())] : Entity();
            alive.push_back(scene.CreateEntity("e", parent));
        }
        else if (op < 6)                                                           // change one field of one entity
        {
            TransformComponent& t = alive[pick(alive.size())].Get<TransformComponent>();
            switch (pick(3))
            {
            case 0: t.position = { number(), number(), number() }; break;
            case 1: t.SetEulerDegrees(number() * 9, number() * 9, number() * 9); break;
            default: t.scale = { 1 + std::fabs(number()) / 10, 1 + std::fabs(number()) / 10, 1 + std::fabs(number()) / 10 }; break;
            }
        }
        else if (op < 8) scene.SetParent(alive[pick(alive.size())], pick(4) == 0 ? Entity() : alive[pick(alive.size())]);
        else if (op == 8) alive[pick(alive.size())].Get<TransformComponent>() = alive[pick(alive.size())].Get<TransformComponent>();
        else if (op == 9) alive[pick(alive.size())].Get<TransformComponent>().MarkDirty();
        else if (op == 10 && alive.size() > 30)
        {
            scene.DestroyEntity(alive[pick(alive.size())]);
            alive.erase(std::remove_if(alive.begin(), alive.end(), [](const Entity& e) { return !e.IsValid(); }), alive.end());
        }

        if (pick(4) != 0) continue;                                                // update every few edits, so changes pile up

        // what has to be recomputed: entities that need it, and everything below them
        std::function<bool(Entity)> stale = [&](Entity e)
        {
            if (e.Get<TransformComponent>().NeedsUpdate()) return true;
            Entity parent = scene.GetParent(e);
            return parent.IsValid() && stale(parent);
        };
        size_t expected = 0;
        for (const Entity& e : alive) if (stale(e)) ++expected;

        const size_t written = TransformSystem::Update(scene);
        ++updates; totalRecomputed += written; totalEntities += alive.size();
        if (written != expected) ++wrongCount;
        for (const Entity& e : alive)
            if (!WorldMatches(e, ReferenceWorld(scene, e))) { ++mismatched; break; }
        if (TransformSystem::Update(scene) != 0) ++wrongCount;                     // and a second run has nothing to do
    }
    CHECK(mismatched == 0);
    CHECK(wrongCount == 0);
    CHECK(updates > 500);
    CHECK(totalRecomputed < totalEntities);            // it really did skip entities that had not changed
    if (mismatched || wrongCount) std::printf("  (%d updates; %d with a wrong matrix, %d with a wrong count)\n", updates, mismatched, wrongCount);
}

// The roadmap's exit check for P1: 100 000 entities, one parent moves, the update costs a few milliseconds.
// The best of 20 runs is compared with the budget, so a busy machine does not fail it. Where this was written one moving
// parent took ~0.9 ms and recomputing all 100 000 matrices (what the old system did every frame, plus a depth sort) ~7 ms.
static void TestTransformUpdateOf100kEntitiesWithOneMovingParent()
{
    Scene scene;
    Entity mover;
    for (int g = 0; g < 1000; ++g)                                                 // 1000 groups of 1 + 99 entities
    {
        Entity group = scene.CreateEntity("group");
        group.Get<TransformComponent>().position = { float(g), 0, 0 };
        if (g == 0) mover = group;
        for (int i = 0; i < 99; ++i) scene.CreateEntity("part", group).Get<TransformComponent>().position = { 0, float(i), 0 };
    }
    CHECK(scene.EntityCount() == 100000);

    using Clock = std::chrono::steady_clock;
    const auto t0 = Clock::now();
    CHECK(TransformSystem::Update(scene) == 100000);                               // the first update computes everything
    const double first = std::chrono::duration<double>(Clock::now() - t0).count();
    CHECK(TransformSystem::Update(scene) == 0);

    double best = 1e9;
    for (int run = 0; run < 20; ++run)
    {
        mover.Get<TransformComponent>().position.y = float(run + 1);
        const auto start = Clock::now();
        const size_t written = TransformSystem::Update(scene);
        best = std::min(best, std::chrono::duration<double>(Clock::now() - start).count());
        CHECK(written == 100);                                                     // the group and its 99 parts, nothing else
    }
    CHECK(Near(scene.GetChildren(mover)[98].Get<TransformComponent>().WorldPosition().y, 20 + 98));
    CHECK(best < 0.005);                                                           // 5 ms
    std::printf("  (100k entities: first update %.1f ms, one moving parent %.3f ms best of 20)\n", first * 1000, best * 1000);
}

static void TestTransformIsCurrentAndNaN()
{
    Scene scene;
    CHECK(TransformSystem::IsCurrent(scene));                                       // an empty scene is current
    Entity a = scene.CreateEntity("a"), b = scene.CreateEntity("b", a);
    CHECK(!TransformSystem::IsCurrent(scene));                                      // new entities have no world yet
    TransformSystem::Update(scene);
    CHECK(TransformSystem::IsCurrent(scene));

    a.Get<TransformComponent>().position.x = 1;
    CHECK(!TransformSystem::IsCurrent(scene));                                      // a field changed: the renderer would draw stale data
    CHECK(TransformSystem::IsCurrent(scene) == false && TransformSystem::Update(scene) == 2);
    CHECK(TransformSystem::IsCurrent(scene));

    scene.SetParent(b, Entity());
    CHECK(!TransformSystem::IsCurrent(scene));                                      // a new parent counts too
    TransformSystem::Update(scene);

    // A NaN is seen once and then counts as unchanged; it must not make the scene "never current" (and cost a recompute
    // of its subtree every frame).
    b.Get<TransformComponent>().position.y = std::nanf("");
    CHECK(TransformSystem::Update(scene) == 1);
    CHECK(TransformSystem::IsCurrent(scene) && TransformSystem::Update(scene) == 0);
    b.Get<TransformComponent>().position.y = 2;                                     // and a real value brings it back
    CHECK(TransformSystem::Update(scene) == 1 && Near(b.Get<TransformComponent>().WorldPosition().y, 2));
}

int main()
{
    struct { const char* name; void (*fn)(); } tests[] = {
        { "world from local",                         TestWorldFromLocal },
        { "parent/child composition",                 TestParentChildComposition },
        { "parent rotation affects child",            TestParentRotationAffectsChild },
        { "deep hierarchy, out-of-order creation",    TestDeepHierarchyCreatedOutOfOrder },
        { "cycles are rejected",                      TestCyclesAreRejected },
        { "destroy + stale handles are safe",         TestDestroyRemovesDescendantsAndStaleHandlesAreSafe },
        { "add / replace / remove component",         TestComponentsAddReplaceRemove },
        { "find by name, children",                   TestFindAndChildren },
        { "5000 children under one parent",           TestManyEntitiesUnderOneParent },
        { "destroy a large subtree",                  TestDestroyLargeSubtree },
        { "children are in attach order",             TestChildrenAreInAttachOrder },
        { "detach first / middle / last",             TestDetachFirstMiddleLast },
        { "reparent",                                 TestReparent },
        { "destroy unlinks from the parent",          TestDestroyUnlinksFromParent },
        { "hierarchy vs a reference model (random)",  TestHierarchyAgainstReferenceModel },
        { "child lookup does not scan the scene",     TestChildLookupDoesNotScanTheScene },
        { "transforms: only what changed",            TestTransformRecomputesOnlyWhatChanged },
        { "transforms vs from-scratch (random)",      TestTransformUpdateAgainstReference },
        { "transforms: 100k entities, one parent",    TestTransformUpdateOf100kEntitiesWithOneMovingParent },
        { "transforms: IsCurrent, NaN settles",       TestTransformIsCurrentAndNaN },
    };
    for (auto& t : tests)
    {
        const int before = g_failed;
        t.fn();
        std::printf("%s  %s\n", g_failed == before ? "PASS" : "FAIL", t.name);
    }
    std::printf(g_failed ? "\n%d check(s) FAILED\n" : "\nAll scene tests passed\n", g_failed);
    return g_failed ? 1 : 0;
}
