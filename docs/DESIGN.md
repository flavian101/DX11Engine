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
* Middle mouse = look around (cursor captured). It only starts when the UI does not want the mouse, and while it runs the UI ignores the mouse completely (the pointer is re-centred every move).
* Code that **polls** input (`Input::IsKeyPressed`, used by the Sandbox shortcuts and the camera) does not see events, so it asks: `ImGuiLayer::WantsTextInput()` (a text field is being edited) before reacting to keys.
* The renderer owns its settings (environment, wireframe, culling). Anything that shows or changes them reads the live value first (`Renderer::GetEnvironment`, `IsWireframeEnabled`), so a key and a panel cannot undo each other.

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
* Tests that need no GPU: `scene_tests` (9), `render_tests` (10), `light_tests` (10), `cluster_tests` (11), `shadow_tests` (29), `material_tests` (28) and `rhi_tests` (5), built with `-DDXENGINE_BUILD_TESTS=ON`.
* Also removed along the way: `FrameTime` passed by value as if it were a number (now a plain float of seconds).

## Order of work

| # | Milestone | Leaves behind |
|---|---|---|
| 1 | Scene foundation (EnTT, components, transforms) + render extraction + Sandbox ported | `Model` subclasses and `Renderer::Submit(model)` removed |
| 2 | Lights as components, clustered forward | arbitrary light counts |
| 3 | Shadows (implemented, see Status) | directional + spot + point shadows |
| 4 | Material/shader redesign (implemented, see Status) | one slot table, sRGB, shared surface function |
| 5 | Debug panel, then editor | inspectable, editable scenes |
| 6 | Asset pipeline, animation review | loader emits assets |
