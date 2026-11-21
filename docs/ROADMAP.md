# DXEngine: source of truth

This is the only design document. It says what the engine is, which rules the code follows, what is known to be wrong,
and what is built next, in that order. If the code and this file disagree, one of them is a bug: fix it in the same commit.

What does not belong here: patch numbers, diary-style history (that is `git log`), wish lists without an exit
criterion, and anything a test or a header comment already states better.

Scope: a Windows, Direct3D 11 engine with a scene editor. One build system is supported: CMake (see the README).
The two `.vcxproj` files are kept in step by hand for Visual Studio users; there is no `.sln` in the repository.

---

## 1. What exists

```
Editor (exe)          the application: EditorLayer builds a scene in C++ and shows it through the editor UI
editor/               panels + GPU-free editor logic (selection, hierarchy model, scene editing, viewport math)
Scene                 entt registry + Entity handle + components            scene/
Systems               Transform, Animation, Extraction, Light, Shadow       scene/, renderer/Systems/
Renderer              FrameContext + RenderGraph + passes                   renderer/
RHI                   IGraphicsDevice, D3D11 backend                        RHI/
```

Today `editor/` is compiled into the engine library (see issue D1).

### One frame

```
Application::Render
  layers OnUpdate        the scene is drawn:  BeginScene(camera) -> Submit(scene) -> EndScene()
  ImGuiLayer::Begin
  layers OnUIRender      editor panels
  ImGuiLayer::End        draws into the back buffer, over the scene
  Renderer::Present
```

Inside `EndScene`:

```
CPU systems (fill the FrameContext)
  SceneExtraction   scene + camera -> batches (background / opaque / transparent), culling, sorting
  ShadowSystem      which lights cast, atlas tiles, casters per view        (before LightSystem: light buffers point at its views)
  LightSystem       lights + the light cluster grid
GPU passes (read the FrameContext), in insertion order
  ShadowPass -> ClearPass -> DepthPrepass -> ForwardPass(background) -> ForwardPass(opaque) -> ForwardPass(transparent)
```

The scene lands in `FrameContext::targets` (the back buffer, or a texture when the editor asked for a viewport size
with `Renderer::SetSceneTargetSize`; applied at the next `BeginScene`, never mid-frame).

### Shader register map

| Register | What | Filled by |
|---|---|---|
| b0..b2 | transform (incl. `ReceiveShadows`), bones, material | `MeshDrawer` |
| b3 | `SceneLightData`: environment, directional lights (max 4), local light count | `LightSystem` |
| b5 | `ShadowData`: atlas size, PCF radius | `ShadowSystem` |
| b7 | `ClusterData`: grid size, culling mode, how a pixel finds its cluster | `LightSystem` |
| t0..t13 | material textures: one row per slot in `MaterialSlots.cpp`; declarations are generated and prepended to every shader | `MeshDrawer` |
| t32 | `StructuredBuffer<LightGPU>`: point + spot lights | `LightSystem` |
| t33 / t34 | cluster ranges `{offset, count}` / cluster light indices | `LightSystem` |
| t35 / t36 | shadow atlas (`Texture2D<float>`, D32_FLOAT) / `StructuredBuffer<ShadowView>` | `ShadowPass` / `ShadowSystem` |
| s0 / s1 | default sampler / shadow comparison sampler | `MeshDrawer` |

Engine-wide resources live at t32 and up; t0..t31 belong to material textures. `ClusterConstants` and `LightGPU` are
declared twice (`utils/ConstantBufferTypes.h`, `common.hlsli`); the C++ side `static_assert`s sizes and offsets.

### Who owns what

| Thing | Owner | Lifetime |
|---|---|---|
| Device, ResourceCache, ShaderCache, MaterialSystem | `RenderServices` inside `Renderer` | whole run |
| Pipelines, shared constant + structured buffers, default sampler | `ResourceCache` | whole run (a structured buffer may be replaced by a bigger one) |
| Back buffer, depth buffer | the device | recreated on resize: re-fetch every frame, never cache |
| Scene texture + its depth | `Renderer` | replaced when the requested size changes |
| Camera, batches, stats, pointers to the buffers above | `FrameContext` | one frame |
| Meshes, materials, textures | whoever holds the `shared_ptr` | until the last holder lets go |

### RHI ownership (this replaces the old, self-contradicting note)

1. `IGraphicsDevice::Create*` returns a `shared_ptr`. The caller shares ownership; the device keeps only what it needs
   itself (swap chain, back and depth buffer).
2. Whoever uses a resource holds a `shared_ptr` for as long as it uses it. Nothing may outlive the device: layers and
   anything holding GPU resources are destroyed before `Renderer::Shutdown` (`LayerStack::Clear`).
3. Commands take raw pointers: the resource must outlive command execution (until `Submit` returns).
4. No long-term raw pointers in application code.

---

## 2. Rules the code follows

**Principles**
1. A thing is the components it has. No class hierarchy for game objects.
2. One source of truth: a fact lives in one place and everything else is derived (the texture mask is derived from the
   textures a material holds; the render queue is derived from blend state; there is one slot table and one shading-model table).
3. Assets are shared and immutable; instances are small.
4. Systems transform data: gameplay writes components, systems derive the rest, the renderer only reads.
   (The application runs `TransformSystem::Update` before `Renderer::Submit`; debug builds assert it.)
5. Handles, not raw pointers: a destroyed entity is detected, never dereferenced.
6. Anything that is pure logic is testable without a GPU, and has a test.

**Systems fill the FrameContext, passes read it.** Deciding *what* to draw belongs in a system; creating a GPU object
belongs in `ResourceCache` or a pass. `FrameContext` has typed fields only (no string-keyed bag).

**Input rules (editor)**
* `ImGuiLayer` is the top overlay and marks the events the UI wants as handled; a click on a panel never also picks an entity.
* While the camera is flown, the camera owns the mouse (`ImGuiLayer::SetMouseEnabled(false)`); ImGui's own `WantCaptureMouse`
  stays true while a button pressed over a panel is held, so it cannot be trusted for this.
* The camera asks, per event and per frame, whether it may listen (`CameraController::SetInputGate`).
* Code that polls input asks `ImGuiLayer::WantsTextInput()` before reacting to keys.
* The renderer owns its settings; anything that shows or changes them reads the live value first.

**Editor logic is separate from the UI** and tested without ImGui or a GPU. Panels only call it. `Scene::SetParent` keeps
the local transform, so the editor reparents through `SceneEditing::Reparent`.

**Shading**
* Material colours, light colours and ambient are linear. Colour textures are sRGB formats, data textures are UNORM; the
  colour space belongs to the slot and `ImageLoader` requires it as an argument.
* `ShadingModel` picks the shader file; `RenderState` (blend, cull, depth, queue) picks the pipeline and draw order.
  A material never changes its own shading model.
* One BRDF (`EvaluateDirectLight`), one place that finds lights (`ShadeLocalLights`), one surface evaluation
  (`EvaluateSurface` in `surface.hlsli`).
* Shadows: one atlas texture, all views in one structured buffer; a light gets all its tiles or none; casters are not
  culled by the camera.

**Checklists**
* *Add a pass:* GPU commands -> pass, data preparation -> system. Write `Reads:` / `Writes:` in its comment. If it writes
  something others read, add one typed `FrameContext` field. `graph.AddPass<>` in `Renderer::Init` at the position it runs.
* *Add a material texture slot:* one enumerator before `TextureSlot::Count` (its value is its register) and one row in
  `MaterialSlots.cpp`; use the variable and define in `surface.hlsli`. Everything else follows. `material_tests` checks the table.
* *Add a shading model:* one enumerator and one row in `MaterialTypes.cpp`; the pixel shader is a thin function over
  `EvaluateSurface()`. A test checks that the files exist and that no shader file is unreferenced.

---

## 3. Known problems

Status: **Fixed** = fixed by the cleanup patch series, **Open** = still there. IDs are stable; the roadmap refers to them.

### A. Dead or never-working code
| ID | Problem | Status |
|---|---|---|
| A1 | The custom 2D UI (`UILayer`, `UIManager`, `UIPass`, UI shading model, UI shaders) never rendered: it ran after `EndScene`, where `SubmitUI` ignores calls. It still cost a pass per frame and created example widgets at start-up. | Fixed (removed; `RenderQueue::UI` / `RenderState::Overlay` kept as a state concept) |
| A2 | `ResourceManager/` (never included, could not compile if instantiated), `PerfomanceProfiller.h` (no users), `FrameContext::Set/TryGet` (no users), `IndexData::GenerateAdjacency` (declared, 140 commented-out lines), `MaterialFactory::CreateFromConfig` (stub returning a default material). | Fixed (removed) |
| A3 | Instancing path (`DrawItem::instanceTransforms`, two per-instance loops in `MeshDrawer`, `ShaderFeature::Instancing`) that nothing fed; fog hack in `surface.hlsli` that nothing could enable; unused HLSL defaults and paste-comments in `common.hlsli`. | Fixed (removed) |
| A4 | `ShaderCache::Update` (hot reload) has no callers and `enableHotReload` is false, so shader edits need a restart. | Open (E2) |

### B. Bugs
| ID | Problem | Status |
|---|---|---|
| B1 | `Renderer::Init` failure was silent: every later call became a no-op and the user got a black window. | Fixed (`Init` returns `bool`, the app reports it) |
| B2 | Shutdown order: layers (scene, GPU meshes, textures) were destroyed after `Renderer::Shutdown`; `OnDetach` was never called for them. | Fixed (`LayerStack::Clear` first) |
| B3 | A minimised window spun a CPU core. | Fixed |
| B4 | `CreateCylinder` allocated 32 vertices it never wrote (zero normal) and shaded its caps with the side normals. | Fixed (own cap rings, tests) |
| B5 | `Ray.cpp` included `utils/mesh/...` (wrong case: breaks on a case-sensitive file system). | Fixed |
| B6 | `editor_ui_tests` did not link: the two stubs its comment promises were missing. | Fixed (not run here: needs ImGui and `comdlg32`) |
| B7 | `Esc` quits the whole application at once (unless a text field has focus), and nothing can be saved yet. `Application` also hard-codes the middle-mouse capture (an editor policy living in the engine). | Open (E3) |
| B8 | Assets and shaders are found through paths relative to the working directory (`assets/shaders/`, `assets/textures/...` hard-coded in `EditorLayer`), so the Editor only works when started from the folder holding its assets. | Open (P3: resolve against the executable / a project root) |

### C. Does not scale
| ID | Problem | Status |
|---|---|---|
| C1 | `TransformSystem::Update` ran inside `Renderer::Submit`, for every entity every frame: it walked the depth of each entity and sorted, with no dirty flags. | Fixed (only changed subtrees are recomputed; the application calls `Update`, `Submit` only reads) |
| C2 | `Scene::GetChildren` scanned every entity; `DestroyEntity` was O(entities x descendants). | Fixed (`RelationshipComponent`: parent, first/last child, prev/next sibling; children, detach and subtree destruction cost what they touch; test with a time budget) |
| C3 | The shader variant key is built from strings on every draw. | Open (P4) |
| C4 | Every draw uploads a full transform buffer including the per-frame view/projection; skinned submeshes re-upload 128 bone matrices each; `DrawItem` copies two `shared_ptr`s per draw per frame. | Open (P4) |
| C5 | No GPU instancing. | Open (P4; design the HLSL input first) |
| C6 | About 450 MB of textures and models are tracked in git, and every Editor build copied all of it next to the exe. | Copy Fixed (incremental with CMake >= 3.26); history size Open (P1: Git LFS or trimmed assets) |
| C7 | The scene target is 8-bit and every pixel shader tone maps and encodes its own output: no HDR, no exposure, blending and any later bloom or IBL work on tone-mapped values. HDR images are stored as 8 bits. | Open (P4) |
| C8 | `Renderer` is a global static: no second view (material preview, second viewport), no headless instance. | Open (P2) |

### D. Architecture
| ID | Problem | Status |
|---|---|---|
| D1 | About 38 editor files live in the engine library and `DXEngine.h` includes editor headers, so a game build would link ImGui, ImGuizmo and `comdlg32`. | Open (P2) |
| D2 | `DXEngine/tests/CMakeLists.txt` listed engine sources by hand, repeated per test target, so any file move touched many targets. | Fixed (every test links `DXEngineCore`; no engine source is listed in the tests file). Link-checked with a MinGW cross build, not yet built with MSVC |
| D3 | Components hold `shared_ptr<Mesh>` / `shared_ptr<Material>` and entities have no stable id: a scene cannot be saved or loaded, and a prefab cannot refer to an asset. | Open (P3) |
| D4 | The camera is not a component; its weighted behaviour-blending stack is more than a fly camera needs. | Open (P5) |
| D5 | Play mode only hides the panels: there is no scene snapshot and restore, and no game layer separate from the editor. | Open (P3) |
| D6 | The editor scene is built in C++ (`EditorLayer`) with asset paths relative to the working directory. | Open (P3) |
| D7 | `ShadingModel::Emissive` is Lit plus emission under another name (the "one fact, two places" pattern the design rules out). | Open (P4) |
| D8 | `SceneFactory` takes the device as a parameter while `Renderer::GetDevice()` is a global. `utils/` holds core types (Mesh, Material, Transform). | Open (P2) |

### E. Docs and tooling
| ID | Problem | Status |
|---|---|---|
| E1 | Three overlapping design documents, a README describing a Premake build and a `shaders/` folder that do not exist, and an ownership note that contradicted itself. | Fixed (this file replaces them; README rewritten) |
| E2 | No way to see a shader edit without restarting. | Open (P4) |
| E3 | `LICENSE.txt` is the BSD-3-Clause text with the copyright line of the Premake project (left over from the Premake build), not this project's author. Choosing the copyright holder is the owner's call; the file is untouched. | Open (owner) |

---

## 4. Roadmap

Each phase ends with something a person can check. Do not start a phase with a failing exit check in the previous one.

**P1: Foundations** (C1, C2, C6, D2)
* Transforms, recompute only what changed: *done.* An entity is dirty when its position / rotation / scale differ from the
  values its world matrix was computed from (so the inspector, gizmo, animation and gameplay keep writing the fields and
  nothing has to remember a flag), or when `MarkDirty()` was called (`SetParent` does; a copied `TransformComponent` always
  starts dirty). `Update` recomputes each topmost dirty entity with its subtree once, following the child links, and returns
  how many matrices it wrote. 100 000 entities with one moving parent: about 1 ms (test with a time budget).
* Transforms, `TransformSystem::Update` called by the application before `Renderer::Submit`: *done.* `EditorLayer::OnUpdate`
  calls it after `AnimationSystem`; `Renderer::Submit` only reads, and a debug build asserts `TransformSystem::IsCurrent`.
* Hierarchy: *done.* `RelationshipComponent` (parent, first/last child, prev/next sibling) replaced the `ParentComponent` scan;
  child iteration, unparenting and subtree destruction need no scan. Children are kept in attach order.
* Build: *done.* One `DXEngineCore` static library (a glob of everything under `DXEngine/src`; it has no entry point, `Main.h`
  is a header) that the Editor and every test link; `DXEngine` remains as an alias. `tests/CMakeLists.txt` is one
  `dx_add_test(name)` line per test plus its definitions. A linker takes from a static library only the files a test
  reaches, so each test still pulls in only what it uses (and `editor_ui_tests` still never links the renderer: it stubs
  two functions). The price: building any test first builds the whole engine, assimp and ImGui. (The editor is still
  inside the library; P2 moves it out.)
* Assets in git: move the large textures and models to Git LFS, or keep only small stand-ins in the repository.
* *Exit:* a scene with 100k entities and one moving parent updates in a few milliseconds (a test with a time budget);
  `tests/CMakeLists.txt` has no engine `.cpp` listed more than once (now: none at all).

**P2: Separate the editor, make the renderer an instance** (C8, D1, D8)
* `DXEngineEditor` library (or `Editor/src`) holds `editor/`; `DXEngine.h` stops including it.
* `Renderer` becomes an object owned by `Application`; a `RenderView` (camera + target + scene) lets the editor render
  more than one view. The global facade can stay as a thin wrapper until nothing uses it.
* `SceneFactory` and friends take what they need explicitly. Core types leave `utils/`.
* *Exit:* a minimal "Game" executable links the engine without ImGui; the editor shows two views of one scene.

**P3: Assets, scenes, play mode** (B8, D3, D5, D6)
* `UUID` component on every entity; asset handles (id + path) replace `shared_ptr` in components; an asset registry
  resolves them.
* Scene serialisation (text format) and load/save in the editor; the hard-coded scene becomes a file.
* Play mode snapshots the scene, runs a game layer on a copy, restores on stop.
* Undo / redo and multi-selection (the two things the editor lacks after saving).
* *Exit:* save, quit, start, load gives the same scene (round-trip test); Play then Stop restores every component.

**P4: Renderer scale and quality** (A4, C3, C4, C5, C7, D7, E2)
* Integer-hashed variant key; per-frame constants (view, projection, camera, time) in one buffer, per-object data in a
  structured buffer indexed by instance id; `DrawIndexedInstanced` for identical mesh + material.
* RGBA16F scene target, one tone-mapping pass with exposure, HDR environment textures and image-based lighting.
* Call `ShaderCache::Update` from the editor (hot reload on, with a visible compile error instead of a crash).
* Fold `Emissive` into `Lit` (emissive factor and map are already part of `EvaluateSurface`).
* *Exit:* 10k identical cubes draw in a handful of draw calls; a shader edit shows without a restart; GPU frame-time
  numbers recorded for clustered vs unclustered lighting (still open from the lighting work).

**P5: Gameplay basics** (B7, D4)
* `CameraComponent`; the editor fly camera is a controller, not part of the engine camera.
* Input actions (named, rebindable) instead of polling keys in layers; `Esc` stops play mode, it does not quit.
* A game layer / runtime loop separate from the editor layer.
* Physics and audio are decided only when a game needs them.

---

## 5. Verification status

* GPU-free logic has tests (`DXEngine/tests`, build with `-DDXENGINE_BUILD_TESTS=ON`, run with `ctest`): scene, render
  extraction, lights, clusters, shadows, materials, RHI types, editor logic, mesh optimizer, GPU timings, assets, asset
  catalog, skeleton, image decode.
* The P1 build change (`DXEngineCore`, `dx_add_test`) was built end to end with a MinGW-w64 cross build of the real top-level
  CMake (the library, the Editor and all 17 test executables link) and the GPU-free tests were run on Linux against the same
  sources. It has **not** been built with MSVC or run on Windows: do that first (`-DDXENGINE_BUILD_TESTS=ON`, `ctest`).
* **Never executed on a real GPU or window** (check by running the Editor): the depth pre-pass, the shadow bias values
  (`normalBias`, `constantBias`, `rasterDepthBias`, `rasterSlopeBias` were chosen by reasoning), the editor's look,
  a middle-button fly, the gizmo handles, play mode, and the HLSL changes of the cleanup series (removed fog and unused defines).
* The editor panels are run through real ImGui frames headlessly (`editor_ui_tests`); that cannot say whether the UI looks right.

## 6. Deliberately not built yet

Deferred / GBuffer rendering, post-processing beyond tone mapping, a command queue and multi-threaded recording, buffer
pooling, cone-shaped light culling for spot lights, cascade blending, shadows from transparent or alpha-tested
materials, alpha cut-out materials (the `ShaderFeature::AlphaTest` path exists, nothing sets it), two-sided materials,
streaming of the sky and moon textures, background import.
