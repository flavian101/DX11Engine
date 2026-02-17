
# Modern Modular Renderer Architecture


## New Modular Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                     Renderer (Facade)                        │
│  - Owns subsystems                                          │
│  - Coordinates rendering pipeline                           │
│  - Minimal business logic                                   │
└─────────────────────────────────────────────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│ RenderGraph   │    │ CommandQueue  │    │ ResourceCache │
│ - Render passes│    │ - Command     │    │ - Pipelines   │
│ - Dependencies│    │   recording   │    │ - Buffers     │
│ - Scheduling  │    │ - Submission  │    │ - Textures    │
└───────────────┘    └───────────────┘    └───────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              │
        ┌─────────────────────┼─────────────────────┐
        │                     │                     │
        ▼                     ▼                     ▼
┌───────────────┐    ┌───────────────┐    ┌───────────────┐
│ RenderPass    │    │ RenderBatcher │    │ CullingSystem │
│ - Shadow      │    │ - Instancing  │    │ - Frustum     │
│ - GBuffer     │    │ - Sorting     │    │ - Occlusion   │
│ - Forward     │    │ - Merging     │    │ - Distance    │
│ - UI          │    │               │    │               │
└───────────────┘    └───────────────┘    └───────────────┘
        │                     │                     │
        └─────────────────────┼─────────────────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │ RHI Abstraction   │
                    │ (Unchanged)       │
                    └───────────────────┘
```

## Core Components

### 1. **Renderer** (Facade - 100 lines)
- Coordinates subsystems
- Public API
- Minimal logic

### 2. **RenderGraph** (Render Pass Management)
- Define render passes
- Automatic dependency resolution
- Resource lifetime management
- Parallel pass execution

### 3. **RenderPass** (Interface)
- Shadow pass
- GBuffer pass
- Forward pass
- Transparent pass
- Post-processing
- UI pass

### 4. **CommandQueue** (Command Recording)
- Multi-threaded command recording
- Command buffer pooling
- Submission scheduling

### 5. **RenderBatcher** (Batching System)
- Instance batching
- Material batching
- Static batching
- Dynamic batching

### 6. **CullingSystem** (Visibility Determination)
- Frustum culling
- Occlusion culling
- Distance culling
- Portal culling

### 7. **ResourceCache** (Resource Management)
- Pipeline cache
- Buffer pools
- Texture cache
- Shader variants

### 8. **SceneRenderer** (Scene-specific logic)
- Forward renderer
- Deferred renderer
- Forward+ renderer
- Clustered renderer

## File Structure

```
src/renderer/
├── Renderer.h/cpp                    # Facade (100 lines)
├── core/
│   ├── RenderGraph.h/cpp            # Pass orchestration
│   ├── RenderPass.h/cpp             # Base pass interface
│   ├── CommandQueue.h/cpp           # Command recording
│   └── FrameContext.h/cpp           # Per-frame data
├── passes/
│   ├── ShadowPass.h/cpp             # Shadow rendering
│   ├── GBufferPass.h/cpp            # Deferred GBuffer
│   ├── ForwardPass.h/cpp            # Forward rendering
│   ├── TransparentPass.h/cpp        # Transparency
│   ├── SkyboxPass.h/cpp             # Skybox
│   ├── UIPass.h/cpp                 # UI rendering
│   └── PostProcessPass.h/cpp        # Post-processing
├── systems/
│   ├── RenderBatcher.h/cpp          # Batching logic
│   ├── CullingSystem.h/cpp          # Visibility
│   ├── LightCuller.h/cpp            # Light culling
│   └── SortingStrategy.h/cpp        # Sort algorithms
├── cache/
│   ├── PipelineCache.h/cpp          # Pipeline caching
│   ├── BufferPool.h/cpp             # Buffer pooling
│   └── ResourceManager.h/cpp        # Resource lifetime
└── scene/
    ├── SceneRenderer.h/cpp          # Scene-specific
    ├── ForwardRenderer.h/cpp        # Forward pipeline
    └── DeferredRenderer.h/cpp       # Deferred pipeline
```

## Usage Comparison

### Monolithic (Before):
```cpp
// Everything in one class
Renderer renderer;
renderer.Initialize(device);
renderer.BeginFrame(camera, deltaTime);
renderer.Submit(model);
renderer.EndFrame();
// 47 methods, 2000+ lines, hard to customize
```

### Modular (After):
```cpp
// Build render graph
RenderGraph graph;
graph.AddPass<ShadowPass>("shadows");
graph.AddPass<GBufferPass>("gbuffer");
graph.AddPass<ForwardPass>("forward");
graph.AddPass<UIPass>("ui");
graph.Compile();

// Renderer uses graph
Renderer renderer(device);
renderer.SetRenderGraph(graph);
renderer.BeginFrame(camera, deltaTime);
renderer.Submit(model);
renderer.EndFrame();

// Easy to customize - just swap passes!
graph.ReplacePass<DeferredPass>("forward", "deferred");
```

