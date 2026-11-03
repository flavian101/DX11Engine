# Renderer architecture (replaces the old Readme.md)

The old readme described things that do not exist (parallel pass execution, multi-threaded
command recording, resource lifetime management). This one describes what the code does.

## One frame

```
Renderer::BeginScene(camera)        fills FrameContext: camera, cmd, targets
Renderer::Submit(model) ...         appends to FrameContext::scene
Renderer::EndScene()
   |
   |-- CPU SYSTEMS  (write the blackboard)
   |     CullingSystem   scene       -> visibility
   |     RenderBatcher   visibility  -> batches (background / opaque / transparent)
   |     LightSystem     frustum     -> lightBuffer
   |
   |-- GPU PASSES   (read the blackboard), in insertion order
   |     ClearPass
   |     ForwardPass(background)      skybox
   |     ForwardPass(opaque)
   |     ForwardPass(transparent)
   |     UIPass
   |
   `-- cmd.End -> device.Submit -> device.Present
```

## The one rule that keeps it clean

**Systems fill the FrameContext. Passes read it.**
If you are about to write code that decides *what* to draw inside a pass, it belongs in a system.
If you are about to create a GPU object inside a system, it belongs in `ResourceCache` or a pass.

## Who owns what

| Thing | Owner | Lifetime |
|---|---|---|
| Device, ResourceCache, ShaderCache, MaterialSystem, LightManager | `RenderServices` inside `Renderer` | whole run |
| Pipelines, shared constant buffers, default sampler | `ResourceCache` | whole run |
| Back buffer, depth buffer | the device | recreated on resize, so FrameContext holds raw pointers re-fetched every frame |
| Scene, visibility, batches, stats | `FrameContext` | one frame |

## Adding a pass (checklist)

1. Does it issue GPU commands? -> pass. Does it only prepare data? -> system.
2. Write its `Reads:` / `Writes:` in the class comment.
3. If it writes something others read, add ONE typed field to `FrameContext` and update the "who writes what" table there.
4. `graph.AddPass<MyPass>("name")` in `Renderer::Init`, in the position it should run.

## Deliberately not built yet (add when something needs it)

ShadowPass, GBuffer/deferred, PostProcess, command queue / multithreading, buffer pooling,
true GPU instancing (the HLSL has no per-instance input yet; `MeshDrawer` falls back to one draw per instance).
