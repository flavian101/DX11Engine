# Engine tests (no GPU, no window). Enable with:  cmake -DDXENGINE_BUILD_TESTS=ON ...
# Run:  ctest --output-on-failure
add_executable(scene_tests
    scene_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp)
target_include_directories(scene_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(scene_tests PRIVATE cxx_std_17)
add_test(NAME scene_tests COMMAND scene_tests)

# Renderer-side logic that needs no GPU: scene extraction (culling, routing, sorting, materials).
# Uses the real Mesh / Material classes; meshes are created without a device.
add_executable(render_tests
    render_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/SceneExtraction.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Resource/MeshResource.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/VertexAttribute.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/IndexData.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshOptimizer.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(render_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(render_tests PRIVATE cxx_std_17)
target_link_libraries(render_tests PRIVATE meshoptimizer)
add_test(NAME render_tests COMMAND render_tests)

# Light packing: Scene lights -> the GPU light data (directional block + structured local lights).
add_executable(light_tests
    light_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/LightPacking.cpp)
target_include_directories(light_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(light_tests PRIVATE cxx_std_17)
add_test(NAME light_tests COMMAND light_tests)

# Clustered light assignment (grid maths, light -> cluster, the shader's pixel -> cluster formula).
add_executable(cluster_tests
    cluster_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/LightClustering.cpp)
target_include_directories(cluster_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_compile_features(cluster_tests PRIVATE cxx_std_17)
add_test(NAME cluster_tests COMMAND cluster_tests)

# RHI rules that need no device (buffer description validation).
add_executable(rhi_tests rhi_tests.cpp)
target_include_directories(rhi_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_compile_features(rhi_tests PRIVATE cxx_std_17)
add_test(NAME rhi_tests COMMAND rhi_tests)

# Shadows: cascade fitting, spot / point views, the atlas allocator, and (with the planner) which lights get shadows.
add_executable(shadow_tests
    shadow_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/SceneExtraction.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/LightClustering.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/LightPacking.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/ShadowMath.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Systems/ShadowPlanner.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Resource/MeshResource.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/VertexAttribute.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/IndexData.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshOptimizer.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(shadow_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(shadow_tests PRIVATE cxx_std_17)
target_link_libraries(shadow_tests PRIVATE meshoptimizer)
add_test(NAME shadow_tests COMMAND shadow_tests)

# Materials: the texture slot table (generated #defines / HLSL declarations / colour spaces), Material's derived texture mask
# and the colour-space aware mip generation.
add_executable(material_tests
    material_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/ImageMips.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_compile_definitions(material_tests PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/Editor/assets/shaders/")
target_include_directories(material_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(material_tests PRIVATE cxx_std_17)
add_test(NAME material_tests COMMAND material_tests)

# Editor logic that needs no UI: selection, the hierarchy rows, duplicate / reparent (keeps the world position) / gizmo
# write-back, Euler editing, viewport maths, the gizmo interface, entity bounds, picking (the ray follows the camera).
add_executable(editor_tests
    editor_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/SceneEditing.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/HierarchyModel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EntityBounds.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EditorPrefs.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/ScenePicking.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/picking/Ray.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/camera/Camera.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Resource/MeshResource.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/VertexAttribute.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/IndexData.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshOptimizer.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(editor_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(editor_tests PRIVATE cxx_std_17)
target_link_libraries(editor_tests PRIVATE meshoptimizer)
add_test(NAME editor_tests COMMAND editor_tests)

# The editor panels run through real ImGui frames with no window and no GPU (ImGui's assertions are the oracle:
# unbalanced Begin/End, push/pop, ...). Links the ImGui core; the renderer is not linked (two functions are stubbed in the test).
add_executable(editor_ui_tests
    editor_ui_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/ScenePicking.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/picking/Ray.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/camera/Camera.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/camera/CameraController.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/camera/FreeLookBehavior.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/camera/MovementBehavior.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/SceneEditing.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/HierarchyModel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EntityBounds.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EditorContext.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/HierarchyPanel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/InspectorPanel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/ViewportPanel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/ImGuizmoGizmo.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EditorTheme.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EditorIcons.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/PropertyGrid.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/EditorChrome.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/RendererPanel.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Mesh.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Resource/MeshResource.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/VertexAttribute.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/IndexData.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshOptimizer.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(editor_ui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_link_libraries(editor_ui_tests PRIVATE ImGui ImGuizmo meshoptimizer)
target_compile_features(editor_ui_tests PRIVATE cxx_std_17)
add_test(NAME editor_ui_tests COMMAND editor_ui_tests)

# MeshOptimizer: the engine's wrapper around meshoptimizer, tested against the real library (no engine types, no GPU).
add_executable(mesh_optimizer_tests
    mesh_optimizer_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshOptimizer.cpp)
target_include_directories(mesh_optimizer_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_link_libraries(mesh_optimizer_tests PRIVATE meshoptimizer)
add_test(NAME mesh_optimizer_tests COMMAND mesh_optimizer_tests)

# GpuTimingCollector: the GPU profiler's bookkeeping (header only, no GPU).
add_executable(gpu_timings_tests gpu_timings_tests.cpp)
target_include_directories(gpu_timings_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_compile_features(gpu_timings_tests PRIVATE cxx_std_17)
add_test(NAME gpu_timings_tests COMMAND gpu_timings_tests)

# The RHI parts that need a real D3D11 device: queries, fences, V-Sync, the GPU profiler end to end. Prints SKIPPED and
# passes where no device can be created; run by hand with `ctest -L gpu` after touching src/RHI or the profiler.
set(D3D11_SRC ${CMAKE_SOURCE_DIR}/DXEngine/src/RHI/D3D11)
add_executable(d3d11_device_tests
    d3d11_device_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/renderer/Core/GpuProfiler.cpp
    ${D3D11_SRC}/D3D11Device.cpp ${D3D11_SRC}/D3D11CommandBuffer.cpp ${D3D11_SRC}/D3D11Query.cpp
    ${D3D11_SRC}/D3D11Pipeline.cpp ${D3D11_SRC}/D3D11Shader.cpp ${D3D11_SRC}/D3D11Buffer.cpp
    ${D3D11_SRC}/D3D11Texture.cpp ${D3D11_SRC}/D3D11Sampler.cpp)
target_include_directories(d3d11_device_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_link_libraries(d3d11_device_tests PRIVATE d3d11 dxgi d3dcompiler dxguid)
target_compile_features(d3d11_device_tests PRIVATE cxx_std_17)
add_test(NAME d3d11_device_tests COMMAND d3d11_device_tests)
set_tests_properties(d3d11_device_tests PROPERTIES LABELS gpu)
