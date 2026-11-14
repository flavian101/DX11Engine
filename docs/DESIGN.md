# DXEngine design (v2)

This replaces the per-object `Model` design. It records the decisions, the reasons, and the order of work.
Everything here is built in small steps that each leave the engine runnable.

## Why a redesign

The bugs found while bringing the renderer up all came from the same few patterns:

| Pattern | Where | What it caused |
|---|---|---|
| One fact stored twice (a flag **and** a pointer) | `Model::IsSkinned()` + `m_SkinningData` | access violation, plus an uninitialised flag |
| A subclass used as identity | `Ball`, `Ground`, `LightSphere`, `dynamic_cast` in picking | behaviour hidden in constructors, nothing composable |
| One thing enumerated in several places | texture slot / feature bit / HLSL define / cache mapping | two shifted-by-one bugs (patches 0022, 0046) |
| Objects reaching for globals | `Renderer::GetLightManager()` inside `LightSphere` | hidden initialisation order, a light that was never connected |
| Per-frame "set transform, then submit" | Sandbox | state lives in game code, nothing can be inspected or edited |

## Principles

1. **A thing is what components it has.** No class hierarchy for game objects.
2. **One source of truth.** A fact lives in one place; everything else is derived.
3. **Assets are shared and immutable; instances are small.** Meshes, materials, textures are loaded once; an entity only holds references and its own state.
4. **Systems transform data.** Gameplay writes components, systems (transform, animation, extraction) derive the rest, the renderer only reads.
5. **Handles, not raw pointers.** A destroyed entity is detected, never dereferenced.
6. **Testable without a GPU.** Anything that is pure logic (scene, transforms, light assignment) has tests that run anywhere.

## Layers

```
Game / Sandbox        creates entities, writes components, owns gameplay
Scene                 entities + components (EnTT)            <- scene/
Systems               Transform, Animation, Extraction        <- scene/, renderer/Systems
Renderer              FrameContext + RenderGraph + passes     <- renderer/ (already in place)
RHI                   IGraphicsDevice, D3D11 backend          <- RHI/
```

## Scene (milestone 1)

* `Scene` owns an `entt::registry`. `Entity` is a small value handle (id + scene) that reports `IsValid()` and returns `nullptr` from `TryGet<T>()` after destruction.
* Components are plain data: `Name`, `Transform` (local TRS + derived world matrix), `Parent`, `MeshRenderer`, `Light`. Later: `Skinned`, `Camera`, `Skybox`, `Collider`.
* `TransformSystem::Update` computes world matrices once per frame, parents before children.
* Existing loaded models are bridged in with `Scene::Instantiate(model)` until the loader produces assets directly.

## Rendering

Keep the `FrameContext` + `RenderGraph` + passes structure (it works and is tested). What changes is the input: instead of `Renderer::Submit(model)` every frame, the renderer **extracts** from the scene: visible `MeshRenderer`s become `DrawItem`s, `Light`s become the light buffer.

## Lights (milestone 2): many lights

The current design has hard caps (4 / 64 / 32), a 9 KB constant buffer, a CPU frustum test per light, and an attenuation model with three hand-tuned numbers.

Plan: **clustered forward shading**.
* Lights are `Light` components with physical-ish parameters (colour, intensity, range); the falloff is inverse-square with a smooth window at `range`.
* All lights go into a `StructuredBuffer<Light>` (thousands, not 64).
* The view frustum is split into a grid of clusters (for example 16 x 9 x 24). A cluster stores a range into a `light index` buffer. First version assigns lights to clusters **on the CPU** (pure math, unit-testable); a compute-shader version can replace it later without changing the shader.
* The pixel shader finds its cluster from screen position and depth and loops only over that cluster's lights.

Needs from the RHI: structured buffers with a shader resource view, `SetBuffer` for SRVs. **Done** (patch 0062).

Decisions made while building it:
* **Directional lights stay in the small constant buffer** (b3). There are at most 4, every pixel needs all of them (nothing to cull), and shadows attach cascade matrices to them. Only point and spot lights go into the structured buffer, so the cluster work only ever deals with lights that have a position and a range.
* Engine-wide buffers live at **t32 and up**; t0..t31 belong to the material textures (`TextureSlot::MaxTextureSlots`). The light buffer is `SRV_Lights = 32`.
* The shader finds lights in ONE function, `ShadeLocalLights()` (common.hlsli), and the BRDF exists once (`EvaluateDirectLight`). Clustering changes that function and nothing else.
* **Cluster grid: 16 x 9 screen tiles x 24 exponential depth slices** (3456 clusters), built by `LightClusterer` (pure CPU, patch 0064). The slice is `floor(ln(viewZ) * scale + bias)`: thin near the camera, thick far away. The cluster boxes depend only on the projection and the screen size, so a moving camera costs nothing; an orthographic camera falls back to "loop over all lights".
* A light reaches a cluster when its **sphere** touches the cluster's box. Spot lights use their bounding sphere for now (a cone test would be tighter; it is an internal change to `LightClusterer`).
* The cluster light list has a fixed capacity (2^20 entries = 4 MB, only the used part is uploaded). Every cluster a light reaches costs one entry, so a few hundred big lights can fill it; then the far slices lose lights first and `RenderStatistics::clusterDropped` says so.
* The pixel finds its cluster from `SV_POSITION`: xy are pixel coordinates, and `w` is **1 / view depth** in a pixel shader (not the depth itself).
* `RenderEnvironment::lightCulling` selects `None` (every pixel loops over all lights, the reference), `Clustered` (default) or `ClusterHeatmap` (a debug view of lights per cluster). Sandbox: **L** cycles them, **K** adds / removes 1000 point lights, **I** shows the statistics.
* The falloff is one function too (`LightDistanceAttenuation`). It is still the old `1 / (1 + d^2)` with the smooth window, so scenes look as before; switching to true inverse-square is a change to that function plus re-tuned intensities.

## Shadows (milestone 3)

* Directional light: **cascaded shadow maps** (up to 4 cascades, fitted to slices of the camera frustum, stable texel snapping).
* Spot lights: one shadow view each; point lights: six faces. All in one **shadow atlas** with a budget (the N most important lights cast shadows).
* A `ShadowPass` (depth only, no pixel shader, depth bias + slope-scaled bias) runs before the forward passes and writes the atlas; the forward shader samples it with a comparison sampler and PCF.
* The depth-texture format work (typeless resource, depth view, shader view) is already done (patch 0040).

Decisions made while building it:
* **The atlas is ONE `Texture2D` (D32_FLOAT, 4096 x 4096 by default), not a texture array.** Every view is a square power-of-two tile drawn with a viewport + scissor. A point light is six tiles, not a cube map: the shader picks the face from the dominant axis of `fragment - light`. This needs no per-slice depth views from the RHI, and a cube lookup's seam handling is replaced by a **padded face field of view**, so a pixel's whole PCF kernel stays inside its own tile (tested for every direction).
* **All views live in one structured buffer** (`ShadowViewGPU`, t36); the atlas is t35. A light only stores where its views start: directional -> `shadowViewIndex` (+ `cascadeSplits`, cascade count), spot -> `shadowIndex`, point -> `shadowIndex` = first of 6 faces. The cascade matrices that used to sit in the directional light block (4 x 64 bytes per light, never filled) are gone, so that block shrank from ~1.2 KB to 256 bytes.
* The shader does the matrix multiply from four `float4` rows (`r0*x + r1*y + r2*z + r3`) instead of `mul()`, so nothing depends on HLSL's matrix packing rule and the matrices are NOT transposed. `ShadowMath::MulRows` is the same code on the CPU and is tested against DirectXMath.
* **Planner, then upload, then draw.** `ShadowPlanner` is pure CPU (scene + camera + settings -> views, tiles, casters per view) and is unit-tested; `ShadowSystem` uploads and owns no GPU objects (the `ResourceCache` does); `ShadowPass` only draws. `ShadowSystem` runs BEFORE `LightSystem` because the light buffers point at the views.
* **Casters are not culled by the camera.** An object behind the camera still shadows the view. `SceneExtraction::ExtractShadowCasters` returns every opaque, shadow-casting mesh once, and the planner culls them per view (an oriented box for a cascade, a frustum for a spot / face). Transparent materials and the sky never cast.
* **No shimmer:** a cascade is a sphere fit around its slice (its size does not depend on where the camera looks) and moves in whole-texel steps. Both are tested (a fixed point keeps the same texel phase while the camera walks).
* **Budget, all or nothing:** the strongest sun gets `cascadeCount` tiles; local lights whose reach touches the view are ranked by `range / distance`, the best `maxShadowedLocalLights` get tiles (bigger tiles when they look bigger). A light gets ALL its tiles or none; when the atlas is full the tiles shrink down to `minTileResolution`, then the light is dropped and counted in `shadowLightsDropped`.
* **Bias, in three layers:** a normal offset in the shader (in texels, so it follows the resolution), a small constant bias, and the rasterizer's depth + slope-scaled bias in the shadow pipeline. This needed an RHI fix: `RasterizerState` had depth bias / slope bias / clip / scissor fields that the D3D11 backend ignored (hard-coded values), and a depth-only pipeline could not exist because a pixel shader was required.
* **`receivesShadows`** is a per-draw flag in the transform buffer (`ReceiveShadows`); the shadow lookup is compiled into every lit shader (`ShaderFeature::Shadows`, one variant, not two) and skipped at run time for lights / objects without shadows. `castsShadows` on the light and on the `MeshRenderer` decide who casts.
* The shadow lookup lives inside `CalculateDirectionalLight` / `CalculateLocalLight`, so no pixel shader changed.
* `TextureSlot::Shadow` (14) is no longer used by the shader (it sat inside the material texture range); the atlas is an engine-wide resource at t35.
* Sandbox: a moonlight with cascades, a shadowed spot light on the ship and a shadowed lamp (point light). **H** toggles shadows, **J** cycles the filter (1, 3x3, 5x5, 7x7 taps), **I** shows the shadow counters.
* **Not done on purpose:** shadows from transparent / alpha-tested materials, cascade blending (the last cascade fades out, the borders between cascades are hard), per-frame caching of static shadow maps, a spot-light cone cull of casters (a frustum is used), variance / PCSS soft shadows, a compute-based caster cull.

## Materials and shaders (milestone 4)

* **One table** describing a texture slot: register, HLSL define, HLSL type + name, colour space. Material, shader cache and drawer all read it (removes the whole "kept in sync by hand" bug class).
* Separate **shading model** (Lit, Unlit, ...) from **render state** (blend, cull, depth, queue). A skybox is "sky shading + front-face cull + background queue", not a special material type.
* One shared `EvaluateSurface()` in HLSL; per-model pixel shaders become thin.
* Linear workflow: colour textures as sRGB formats, data textures (normal, roughness, metal, AO) as UNORM.

Decisions made while building it:
* **`MaterialSlots` (utils/material) is the table, and everything is derived from it.** A slot used to be written five times (the `TextureSlot` enum, a `ShaderFeature` bit, a `switch` in `Material::SetTexture`, the feature -> `#define` chain in `ShaderCache`, the `register(tN)` line in `common.hlsli`). Patches 0022 and 0046 were both "one of the five drifted". Now there is one row per slot, and:
  * a Material's **texture mask** (bit i = a texture in slot i) is derived from the textures it holds (`SetTexture` is its only writer, and setting `nullptr` clears the bit), so a flag and a pointer can no longer disagree. The stored `m_FeatureFlags` and `EnableFeature()` are gone;
  * `ShaderCache` keys variants on that mask (`ShaderVariantKey::textureMask`) and **generates both the `#define HAS_*` lines and the `Texture2D x : register(tN)` declarations** that it prepends to every shader. `common.hlsli` no longer declares the material textures. The generated text is `MaterialSlots::GenerateHlslDeclarations()` if you want to read it. `#line 1` follows, so compiler errors count from the shader file's own first line;
  * `ShaderFeature` shrank to vertex attributes + render features; `HAS_DETAIL_TEXTURES` and `ENABLE_PARALLAX_MAPPING` are derived from which slots are bound (a height map switches parallax on; removing it switches it off);
  * `TextureSlot::Shadow` (14) and four never-used reserved slots are gone; `TextureSlot::Count` replaces `MaxTextureSlots`. `HAS_IRRADIANCE_MAP` and `HAS_BRDF_LUT` now exist as real defines (nothing could ever switch them on before).
* **Shading model vs render state.** `MaterialType` (Unlit, Lit, PBR, Transparent, UI, Skybox, Emissive) mixed two questions. Now `ShadingModel` = {Unlit, Lit, Emissive, Sky, UI} picks the shader and `RenderState` = {blend, cull, depth test, depth write, queue} picks the pipeline and the draw order. "PBR" was `Lit` under another name, "Transparent" was `Lit` + alpha blending, so both are gone; a blended unlit hologram needs no new type. `Sky` and `UI` stay models because they compute something different (a cube lookup by direction, screen-space quads), not because of their state.
  * **Derived, never stored:** the render queue (`ResolveRenderQueue`: an explicit queue wins, else blended -> Transparent, else Opaque) and the depth-write decision (`ResolveDepthWrite`: Auto = opaque and a depth test, which is exactly the rule the pipeline cache already used; the tests check every blend x depth-test combination). The pipeline cache, scene extraction and shadow caster extraction all call them.
  * `SkyboxComponent` still decides **placement** (follows the camera, never culled) and forces the background bucket; the material's queue decides the bucket for everything else.
  * **One table gives the shader file names** (`ShadingModels::All()`); `ShaderCache` had five copies of type -> name / files and three name -> type chains (an unknown name silently became UI). A test checks that every named file exists AND that no `.vs.hlsl` / `.ps.hlsl` in the shader folder is unreferenced, so a stray copy cannot creep back.
  * `Material` no longer changes its own model. `SetMetallic()` / `SetRoughness()` used to turn Lit into PBR, and the importer sets a roughness on every material, so **every imported Lit material had silently become PBR**.
* **`surface.hlsli`: `EvaluateSurface()` + `ShadeSurface()` + `ToDisplay()`.** Lit, Unlit and Emissive read the material through the same function. One set of rules (glTF conventions: a material value is a factor, a texture multiplies it, nothing is applied twice). Merging the three copies surfaced real disagreements, all fixed: alpha applied twice in Lit and Transparent; a plain `emissiveColor` ignored by Lit / PBR unless there was an emissive map; the roughness map REPLACED the factor in Lit but multiplied in PBR; the occlusion strength applied twice in PBR and never in Lit; the same texture converted to linear in PBR but not in Lit. `Standard.vs.hlsl` replaces five vertex shaders (the hand-copied `Unlit.vs` had no skinning). `PBR.ps` and `Transparent.ps` are deleted.
  * Dropped on purpose, because they were hidden "transparent look" hacks, not transparency: alpha fading with distance, lighting scaled by 0.8, an extra rim light and lighter tone mapping in `Transparent.ps`. A blended Lit material now lights like any Lit material.
* **Linear workflow.** The colour space belongs to the SLOT (Diffuse, Emissive, DetailDiffuse, Environment, Irradiance are colours -> `RGBA8_SRGB`; everything else is data -> `RGBA8_UNORM`). `ImageLoader` takes it as a **required** argument (a wrong choice does not crash, it just looks subtly off, so there is no default) and `LoadForSlot()` reads it from the table. The importer's texture cache is keyed on path AND colour space (one file can be albedo in one material and data in another). CPU mip chains for sRGB images are filtered in linear light (`ImageMips`: a black/white checkerboard averages to 188, not 128), alpha and data are averaged as plain numbers.
  * Material colours, light colours and ambient are **linear values**. `ApplyGamma(color, 2.2)` is now the exact sRGB curve (the `gamma` setting is a neutral-at-2.2 bend), and `Unlit.ps` encodes its output, so an unlit texture comes out as it is in the file. `PBR.ps` used to apply `pow(color, 2.2)` to textured materials only.
* `MaterialProperties` (a constant buffer mirrored by hand in HLSL) has its size and field offsets pinned with `static_assert`s.
* **Behaviour changes to expect:** untextured PBR materials look brighter (their colour is no longer darkened by a gamma curve that textured ones got); Lit materials now match PBR ones; the night sky is darker and more correct (it was an sRGB image read as linear); imported materials with a small non-zero emissive colour now glow a little (it used to be ignored); imported translucent models are as opaque as their opacity says (they were opacity squared); imported "emissive" models are lit AND glowing instead of flat. The importer takes Assimp colour factors as linear (glTF convention): an OBJ / FBX colour authored in sRGB and WITHOUT a texture will come out brighter than before.
* **Not done on purpose:** an HDR environment format (the cube maps are 8-bit), a BRDF lookup table (the shader still uses the simplified split-sum), per-format colour handling in the importer (sRGB-authored OBJ / FBX colour factors), two-sided materials (needs the back-face normal flipped first), alpha cut-out materials (the `ShaderFeature::AlphaTest` path exists, nothing sets it), moving `TextureNaming`'s file-name guessing onto the slot table.

## Editor (milestone 5)

ImGui is on the **docking** branch (the submodule follows `branch = docking`; run `git submodule update --init` after a pull that moves it).
Everything below sits on top of the scene: the editor is a loop over components, and the engine part knows nothing about game content.

**The frame.** `Application::Render` is: layers `OnUpdate` (the scene is drawn: `BeginScene` ... `EndScene`), then `ImGuiLayer::Begin`, layers `OnUIRender`,
`ImGuiLayer::End` (draws into the back buffer, over the scene), then `Renderer::Present`. `EndScene` no longer presents, which is what makes drawing a UI over the finished scene possible.

**Input rules** (these are easy to get wrong, so they are written down):
* `ImGuiLayer` is the top overlay, so it sees events first and marks the mouse / keyboard events the UI wants as handled. A click on a panel never also picks an entity. Mouse blocking can be switched off (`SetBlockMouseEvents(false)`): the editor does that while the cursor is over its viewport.
* Middle mouse = look around (cursor captured). It only starts when the UI does not want the mouse. While it runs the camera owns the mouse: `ImGuiLayer::SetMouseEnabled(false)` makes the UI let every mouse event through.
  This is NOT left to ImGui's own flags: `io.WantCaptureMouse` stays true for as long as a button that went down over a panel is held, wherever the pointer goes, even with `ImGuiConfigFlags_NoMouse`
  (pinned down by `editor_ui_tests`), which used to swallow every mouse-move the camera needed. The cursor is re-centred when the capture starts (no jump) and put back where it was when it ends.
* The camera asks, per event and per frame, whether it may listen (`CameraController::SetInputGate`; `Editor::SetCamera` installs "play mode, or being flown"): there is no flag that is updated a frame late.
* Code that **polls** input (`Input::IsKeyPressed`, used by the Sandbox shortcuts and the camera) does not see events, so it asks: `ImGuiLayer::WantsTextInput()` (a text field is being edited) before reacting to keys.
* The renderer owns its settings (environment, wireframe, culling). Anything that shows or changes them reads the live value first (`Renderer::GetEnvironment`, `IsWireframeEnabled`), so a key and a panel cannot undo each other.

**Editor logic is separate from the UI** (`editor/`, tested in `editor_tests` with no GPU and no ImGui): `EditorSelection` (an `Entity` value, so a destroyed entity is simply "nothing selected"),
`HierarchyModel` (the tree flattened into the visible rows, O(entities), ordered by entity id so rows do not shuffle), `SceneEditing` (duplicate a subtree, **reparent without moving the entity**, create lights, write a world matrix back as local TRS),
`TransformEditing` (Euler <-> quaternion and a cache that keeps the angles the user typed), `ViewportMath` (panel size -> texture size, mouse -> texture pixel, world point -> pixels).
The panels only call these. `Scene::SetParent` alone keeps the *local* transform, so an editor that used it directly would make entities jump: the editor goes through `SceneEditing::Reparent`.

**The gizmo is ImGuizmo** (submodule `DXEngine/vendor/ImGuizmo`, run `git submodule update --init` after pulling), behind `IGizmo` (`editor/GizmoTypes.h`): `GizmoSettings` (tool, local / world, snapping), a `GizmoView` (DirectXMath matrices + the viewport rectangle) and
`Manipulate(view, settings, world)` which takes the selected entity's world matrix and returns true when the user changed it. `SceneEditing::ApplyWorldMatrix` turns that into the local position / rotation / scale, parent-aware. `ImGuizmoGizmo` is the only code that knows ImGuizmo exists.
Handedness is not an option of ImGuizmo, so it is *measured*, not assumed: `editor_ui_tests` drives it with the engine's left-handed camera (front and diagonal views) and checks that dragging the X, Y, Z handle moves the entity along that axis, toward the mouse, and nowhere else,
and that the whole path works for a child under a rotated, scaled parent. (Rotation and scale handles are not driven by a test: only drawn.) A different widget is a different `IGizmo`; `NullGizmo` (no gizmo) is still there.

**The editor** (`editor/Editor`): a main menu, a toolbar, a status bar, a dock space and four panels, driven by one call from a layer's `OnUIRender`. The dock layout is kept in `editor_layout.ini`
(View > Reset layout restores the default: hierarchy over renderer on the left, inspector on the right, viewport in the middle); UI scale and fly speed are kept in `editor_prefs.ini`.

*Design rules* (what the screens are built on, so new panels look and behave the same):
* **One look.** `EditorTheme` owns every colour, spacing and rounding (flat dark, one blue accent, roomy 5 px frame padding so controls are easy to hit); the axis colours (X red, Y green, Z blue) and the entity-kind colours live there too.
  `Apply(scale)` rebuilds the style from scratch, so the View > UI scale (85 % .. 200 %) can be changed any time and never compounds.
* **One way to show a property.** `PropertyGrid`: a label column and a control column, one row per property, the label column is shared and draggable. `Vec3` has coloured X / Y / Z buttons (click = reset that axis). No panel lays out its own rows.
* **Icons without assets.** `EditorIcons` draws every icon with the draw list (no font, no texture): they stay sharp at any scale.
* **Tools where the work is.** The toolbar has select / move / rotate / scale, world / local, snapping (with its step), wireframe and shadows, and Play. The status bar shows the selection, a hint for what the mouse is over, and FPS / entities / draw calls / triangles.
  The viewport itself stays clean: the picture, the selection outline, the gizmo, and while flying the camera speed.
* **Empty states say what to do** (an empty hierarchy, nothing selected). Anything destructive is reachable by menu and by shortcut; long lists are clipped.

*Panels*
* **Viewport** shows the scene as a texture: every frame it asks `Renderer::SetSceneTargetSize` for the size of the panel (applied at the next `BeginScene`, so a resize shows one frame late), keeps the camera's aspect ratio equal to the panel's
  (`CameraController::SetFollowWindowAspect(false)`), and draws the outline of the selection with the draw list. Left click picks, `F` frames the selection, `Q W E R` choose the tool, `Del` / `Ctrl+D` delete / duplicate.
  Middle button flies the camera; the mouse wheel then changes the fly speed (kept between runs). The camera only listens while it is flown (`CameraController::SetInputGate`), so typing in a field cannot move it.
* **Hierarchy** shows the visible rows through `HierarchyModel` and a list clipper (2000 entities draw like 30), each with a kind icon and, for meshes, an eye that shows / hides them. Double click or `F2` renames in place (Enter keeps, Esc cancels, an empty name is refused).
  Drag an entity onto another to parent it (it stays where it is), onto the empty area to unparent; right click: create child, rename, duplicate, unparent, delete. The search box shows a flat list of matches. Structural changes are collected while the rows are drawn and applied after the loop.
* **Inspector**: the header holds the kind icon, the name and "Add component"; below, Transform (position, rotation in degrees through `EulerCache`, scale with an optional uniform lock), Mesh renderer (flags, mesh info, the materials with a colour swatch each), Light.
  Materials are shared assets and the panel says so; their edits go through `Material`'s setters (which mark the GPU copy outdated).
* **Renderer**: display switches, lighting, shadows, last-frame statistics (the "must be zero" counters are red).
* **Create** (menu, hierarchy `+`, right click): empty, cube / sphere / plane (each with a material of its own), point / spot / directional light. New root objects appear 8 units ahead of the camera, children at their parent's origin. Tools > Stress test adds 1000 point lights under one parent (one delete removes them all).
* Play mode: the Play button or `F1` puts the scene back into the window and hides every panel for as long as you play (a hint fades out after 4 seconds); `F1` returns.

*Not there yet, on purpose*: undo / redo, multi-selection, saving and loading a scene, importing models and textures. The last two are the asset milestone; the first two are the next thing the editor needs after it.

**Tested without a window** (`editor_ui_tests`): the panels run through real ImGui frames with no backend and no GPU; ImGui's own assertions (unbalanced `Begin`/`End`, push/pop, `BeginDisabled`) are the oracle. The tests also *use* the UI the way a person does with injected mouse and key events:
click-pick in the viewport, drag a gizmo handle, select rows, expand, toggle the eye, rename (double click, `F2`, Esc), `Ctrl+D`, `Delete`, drag-and-drop parenting (the world position stays), the theme's scale, the toolbar and status bar in every state.
It cannot say whether the UI *looks* right: that needs a run.

**Picking was wrong, and is fixed.** `RayIntersection::ProjectRay` was written for OpenGL's -Z convention; the camera is a Direct3D left-handed one, so the ray pointed BEHIND the camera and a click found what was behind it. It now unprojects the near and far points under the cursor, and `editor_tests`
checks it against the projection (clicking where an entity is drawn picks it, left/right and up/down included, a camera looking away picks nothing, a non-window-shaped viewport works).
Also fixed on the way: `CameraBehavior::m_IsActive` was never initialised (and `SetActive` did nothing) although `Camera::Update` reads it.

**Controls.** Middle button held: look around, `W A S D` fly (editor and play mode). `F1`: edit <-> play (play = the scene fills the window, no UI). In the editor, with the viewport focused: left click picks, `F` frames the selection, `Q W E R` tool (when a gizmo exists),
`Del` / `Ctrl+D` delete / duplicate, and the Sandbox shortcuts (`T` wireframe, `L` light culling, `K` stress lights, `H` shadows, `J` PCF, `I` statistics, `N` animation clip, ...).

**Debug panel first** (`editor/RendererPanel`): frame time, the renderer's statistics (with the "must be zero" counters in red), wireframe / culling toggles, the environment and every shadow setting.

## Assets and animation (milestone 6)

* `ModelLoader` produces an asset (meshes, materials, skeleton, clips, node tree) instead of a `Model`; `Instantiate` creates entities from it.
* Animation: keep the current hand-written evaluator (tested, small) until blending/layering/IK are needed; then evaluate a library (ozz-animation) as its own step.

## Status

* **Milestone 1 is done** (patches 0051-0061): EnTT scene, transform hierarchy, extraction from the scene, lights and picking as
  scene features, Sandbox rebuilt as a scene. `Ball`, `Ground`, `SkySphere`, `LightSphere`, `LightManager`, `PickingManager` are gone.
  `Model` remains only as the importer's output; `Scene::Instantiate` turns it into entities (skinned models keep animating
  through a transitional bridge).
* **Milestone 2, implemented** (patches 0062 to 0065): structured buffers in the RHI; point/spot lights in a `StructuredBuffer<LightGPU>` (up to 8192, caps of 64 / 32 gone);
  the pure cluster grid and light assignment with tests; the GPU hookup (cluster constants + two structured buffers, the pixel shader looping over its cluster only), a heat-map debug view and a 1000-light stress scene in the Sandbox.
  **Open until it has run on a GPU:** compare frame times with and without clusters (Sandbox: K, then L, with I on). Not done on purpose: cone-shaped culling for spot lights, a compute-shader assignment, a depth pre-pass.
* **Milestone 3, implemented** (patches 0066 to 0070): depth-only pipelines and the missing rasterizer state in the RHI; the pure shadow math (stable cascades, spot / point views, atlas allocator); caster extraction and the `ShadowPlanner`;
  `ShadowSystem` + `ShadowPass` + the depth-only drawer; the shader lookup (cascade pick, normal offset, PCF inside an atlas tile); a shadowed sun, spot and lamp in the Sandbox.
  **Open until it has run on a GPU:** the bias values (`normalBias`, `constantBias`, `rasterDepthBias`, `rasterSlopeBias` in `ShadowSettings`) were chosen by reasoning, not by looking at acne / peter-panning in the Sandbox; tune them with **J** and the three light types. The HLSL was compile-checked with DXC (shader model 6, with and without `ENABLE_SHADOWS`), the C++ was syntax-checked against the Windows headers, the CPU logic is tested; nothing of it has been run through D3D11 and FXC.
* **Milestone 4, implemented** (patches 0071 to 0075): one texture slot table (generated defines + HLSL declarations, derived texture mask); the linear workflow (sRGB / linear textures, linear-light mips, exact sRGB encode); one shared surface evaluation in HLSL; shading model and render state split (`MaterialType` removed).
  **Open until it has run on a GPU:** none of the D3D11 / FXC path has been executed. The HLSL was compile-checked with DXC (shader model 6; 203 variants across texture masks x vertex defines x render defines, built exactly the way `ShaderCache` builds them), the C++ was syntax-checked against the Windows headers, and the CPU logic is tested. Look at the Sandbox for: the ground / moon / ship textures (colour should look right, not washed out or dark), the night sky, the lamp glow, and a transparent imported model.
* **Milestone 5, implemented** (patches 0076 to 0085): ImGui on the docking branch in both build systems; the renderer can draw the scene into a texture and `Present()` is its own call; the `ImGuiLayer` with the input rules; the editor (menu, toolbar, status bar,
  dock space, Hierarchy / Inspector / Viewport / Renderer panels) over GPU-free logic; ImGuizmo as the transform gizmo; the application is the **Editor** (it was the Sandbox) and starts from primitives only: no model is loaded, the ground stays (F1: edit / play).
  Existing bugs fixed on the way, because the editor needs them: picking aimed behind the camera (`ProjectRay`), `CameraBehavior::m_IsActive` was uninitialised, and the camera did not turn in the editor (ImGui keeps `WantCaptureMouse` true while a button pressed over a panel is held).
  **Open until it has run on a GPU:** none of this has been executed with a window. The C++ was syntax-checked against the Windows headers (MinGW), the logic is tested, and the panels were run through real ImGui frames headlessly and *used* with injected mouse and key events (assertions on), but NOT looked at.
  Check: the look (spacing, colours, the icons), the viewport image (colours should match the old full-window view), a middle-button fly (the view turns, the wheel changes the speed, the cursor comes back where it was), a click picks what is under it, the gizmo handles, materials edits show up, the UI scale, play mode and back.
* The start-up time: no model is loaded, and the ground uses the small grass textures (the big 'grass_with_mud_and_stones' set is 80 MB of JPEG). What is left is the sky (6 PNGs) and the 8k moon texture; the asset system should stream those.
* Tests that need no GPU: `scene_tests` (9), `render_tests` (11), `light_tests` (10), `cluster_tests` (11), `shadow_tests` (29), `material_tests` (28), `rhi_tests` (5), `editor_tests` (25) and `editor_ui_tests` (17), built with `-DDXENGINE_BUILD_TESTS=ON`.
* Also removed along the way: `FrameTime` passed by value as if it were a number (now a plain float of seconds).

## Order of work

| # | Milestone | Leaves behind |
|---|---|---|
| 1 | Scene foundation (EnTT, components, transforms) + render extraction + Sandbox ported | `Model` subclasses and `Renderer::Submit(model)` removed |
| 2 | Lights as components, clustered forward | arbitrary light counts |
| 3 | Shadows (implemented, see Status) | directional + spot + point shadows |
| 4 | Material/shader redesign (implemented, see Status) | one slot table, sRGB, shared surface function |
| 5 | Debug panel, then editor (implemented, see Status; gizmo pending) | inspectable, editable scenes |
| 6 | Asset pipeline, animation review | loader emits assets |
