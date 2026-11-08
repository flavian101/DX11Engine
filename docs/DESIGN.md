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

Needs from the RHI: structured buffers with a shader resource view, `SetBuffer` for SRVs.

## Shadows (milestone 3)

* Directional light: **cascaded shadow maps** (3-4 cascades, fitted to the camera frustum, stable texel snapping).
* Spot lights: one shadow map each; point lights: cube (6 faces). All in one **shadow atlas** with a budget (the N most important lights cast shadows).
* A `ShadowPass` (depth only, no pixel shader, depth bias + slope-scaled bias) runs before the forward passes and writes the atlas; the forward shader samples it with a comparison sampler and PCF.
* The depth-texture format work (typeless resource, depth view, shader view) is already done (patch 0040).

## Materials and shaders (milestone 4)

* **One table** describing a texture slot: register, feature bit, HLSL define, default value, colour space. Material, shader cache and drawer all read it (removes the whole "kept in sync by hand" bug class).
* Separate **shading model** (Lit, PBR, Unlit) from **render state** (blend, cull, depth, queue). A skybox is "unlit + front-face cull + background queue", not a special material type.
* One shared `EvaluateSurface()` in HLSL; per-model pixel shaders become thin.
* Linear workflow: colour textures as sRGB formats, data textures (normal, roughness, metal, AO) as UNORM.

## Editor (milestone 5)

Not needed yet, and not before the scene exists. Once everything is an entity with components, an ImGui editor is mostly a loop over components: hierarchy panel, inspector, gizmo, stats. ImGui is already vendored. A small **debug panel** (frame stats, light list, toggles) is worth adding earlier because it is cheap and pays off immediately.

## Assets and animation (milestone 6)

* `ModelLoader` produces an asset (meshes, materials, skeleton, clips, node tree) instead of a `Model`; `Instantiate` creates entities from it.
* Animation: keep the current hand-written evaluator (tested, small) until blending/layering/IK are needed; then evaluate a library (ozz-animation) as its own step.

## Status

* **Milestone 1 is done** (patches 0051-0061): EnTT scene, transform hierarchy, extraction from the scene, lights and picking as
  scene features, Sandbox rebuilt as a scene. `Ball`, `Ground`, `SkySphere`, `LightSphere`, `LightManager`, `PickingManager` are gone.
  `Model` remains only as the importer's output; `Scene::Instantiate` turns it into entities (skinned models keep animating
  through a transitional bridge).
* Tests that need no GPU: `scene_tests` (9) and `render_tests` (13), built with `-DDXENGINE_BUILD_TESTS=ON`.
* Also removed along the way: `FrameTime` passed by value as if it were a number (now a plain float of seconds).

## Order of work

| # | Milestone | Leaves behind |
|---|---|---|
| 1 | Scene foundation (EnTT, components, transforms) + render extraction + Sandbox ported | `Model` subclasses and `Renderer::Submit(model)` removed |
| 2 | Lights as components, clustered forward | arbitrary light counts |
| 3 | Shadows | directional + spot + point shadows |
| 4 | Material/shader redesign | one slot table, sRGB, shared surface function |
| 5 | Debug panel, then editor | inspectable, editable scenes |
| 6 | Asset pipeline, animation review | loader emits assets |
