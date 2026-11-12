# Renderer architecture (replaces the old Readme.md)

The old readme described things that do not exist (parallel pass execution, multi-threaded
command recording, resource lifetime management). This one describes what the code does.

## One frame

```
Renderer::BeginScene(camera)        fills FrameContext: camera, cmd, targets, environment
Renderer::Submit(scene)             makes the world matrices current, points FrameContext at the Scene
Renderer::SubmitUI(element) ...
Renderer::EndScene()
   |
   |-- CPU SYSTEMS  (write the blackboard)
   |     SceneExtraction  scene + camera  -> batches (background / opaque / transparent), culling, sorting
   |     ShadowSystem     scene + env + camera -> which lights cast shadows, atlas tiles, casters per view (b5, t35, t36)
   |                      (runs BEFORE LightSystem: the light buffers point at its views)
   |     LightSystem      scene + env + camera -> lights (b3, t32) and the light cluster grid (b7, t33, t34)
   |
   |-- GPU PASSES   (read the blackboard), in insertion order
   |     ShadowPass                   depth only: every shadow view into its tile of the atlas
   |     ClearPass
   |     ForwardPass(background)      skybox
   |     ForwardPass(opaque)
   |     ForwardPass(transparent)
   |     UIPass
   |
   `-- cmd.End -> device.Submit

Renderer::Present()                 device.Present. Application calls it once per frame, AFTER the UI layer has drawn on top.
```

Where the scene lands: `FrameContext::targets.color` / `depth`. By default that is the window's back buffer. An editor calls
`Renderer::SetSceneTargetSize(w, h)` and the scene is drawn into a texture (`Renderer::GetSceneTexture()`) it shows inside a panel;
the renderer then clears the back buffer itself, because no pass touches it. Passes read `targets.color` and never know which one it is.
The size is applied at the next `BeginScene`, never mid-frame: the UI being built may still point at the old texture.

## Shader register map

| Register | What | Bound by |
|---|---|---|
| b0..b2 | transform (incl. `ReceiveShadows`), bones, material | `MeshDrawer` |
| b3 | `SceneLightData`: environment + directional lights + local light count | `MeshDrawer` (filled by `LightSystem`) |
| b5 | `ShadowData`: atlas size, PCF radius, shadows on/off | `MeshDrawer` (filled by `ShadowSystem`) |
| t0..t13 | material textures: ONE row per slot in `MaterialSlots.cpp`; the declarations are generated and prepended to every shader | `MeshDrawer` |
| b7 | `ClusterData`: grid size, culling mode, how a pixel finds its cluster | `MeshDrawer` (filled by `LightSystem`) |
| t32 | `StructuredBuffer<LightGPU> localLights` (point + spot) | `MeshDrawer` (filled by `LightSystem`) |
| t33 | `StructuredBuffer<uint2> clusterRanges`: {offset, count} per cluster | `MeshDrawer` (filled by `LightSystem`) |
| t34 | `StructuredBuffer<uint> clusterLightIndices`: the lights of every cluster | `MeshDrawer` (filled by `LightSystem`) |
| t35 | `Texture2D<float> shadowAtlas`: every shadow view as a tile (D32_FLOAT) | `MeshDrawer` (rendered by `ShadowPass`) |
| t36 | `StructuredBuffer<ShadowView> shadowViews`: matrix + atlas tile per view | `MeshDrawer` (filled by `ShadowSystem`) |
| s0 | default sampler | `MeshDrawer` |
| s1 | shadow comparison sampler (linear, `<=`) | `MeshDrawer` |

`ClusterConstants` and `LightGPU` are declared twice, in `utils/ConstantBufferTypes.h` and `common.hlsli`. The C++ side `static_assert`s the size of both (and every offset of
`LightGPU`), so a change there fails the build instead of corrupting lights; the shader compiler's reflection lists the HLSL offsets for a manual check.

## The one rule that keeps it clean

**Systems fill the FrameContext. Passes read it.**
If you are about to write code that decides *what* to draw inside a pass, it belongs in a system.
If you are about to create a GPU object inside a system, it belongs in `ResourceCache` or a pass.

## Who owns what

| Thing | Owner | Lifetime |
|---|---|---|
| Device, ResourceCache, ShaderCache, MaterialSystem | `RenderServices` inside `Renderer` | whole run |
| Pipelines, shared constant + structured buffers, default sampler | `ResourceCache` | whole run (a structured buffer may be REPLACED by a bigger one) |
| Back buffer, depth buffer | the device | recreated on resize, so FrameContext holds raw pointers re-fetched every frame |
| Scene texture + its depth (only while an editor asked for one) | `Renderer` | replaced when the requested size changes (applied at `BeginScene`) |
| Camera, batches, stats, pointers to the buffers above | `FrameContext` | one frame |

## Adding a pass (checklist)

1. Does it issue GPU commands? -> pass. Does it only prepare data? -> system.
2. Write its `Reads:` / `Writes:` in the class comment.
3. If it writes something others read, add ONE typed field to `FrameContext` and update the "who writes what" table there.
4. `graph.AddPass<MyPass>("name")` in `Renderer::Init`, in the position it should run.

## Adding a material texture slot (checklist)

1. Add the enumerator to `TextureSlot` (before `Count`; its value is its HLSL register) and ONE row to the table in `MaterialSlots.cpp`: name, HLSL type + variable name, `HAS_*` define, sRGB (a colour) or linear (data).
2. Use the variable and the define in `surface.hlsli` (`EvaluateSurface`). That is all: the declaration, the define, the Material's mask bit, the binding and the loader's colour space all follow from the row. `material_tests` fails if the row is out of order or collides with another.

## Adding a shading model (checklist)

1. Add the enumerator to `ShadingModel` and ONE row to the table in `MaterialTypes.cpp` (name, vertex + pixel shader file, lit by the scene or not). A test checks the files exist, and that no shader file in the folder is unreferenced.
2. Write the pixel shader as a thin function over `EvaluateSurface()` (and `ShadeSurface()` if it is lit). Blending, culling and draw order are NOT part of a model: they are the material's `RenderState`.

## Deliberately not built yet (add when something needs it)

GBuffer/deferred, PostProcess, command queue / multithreading, buffer pooling,
true GPU instancing (the HLSL has no per-instance input yet; `MeshDrawer` falls back to one draw per instance).
