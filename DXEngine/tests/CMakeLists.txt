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
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(render_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(render_tests PRIVATE cxx_std_17)
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
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/Mesh/Utils/MeshBuffers.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_include_directories(shadow_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(shadow_tests PRIVATE cxx_std_17)
add_test(NAME shadow_tests COMMAND shadow_tests)

# Materials: the texture slot table (generated #defines / HLSL declarations / colour spaces), Material's derived texture mask
# and the colour-space aware mip generation.
add_executable(material_tests
    material_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/ImageMips.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialSlots.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/MaterialTypes.cpp)
target_compile_definitions(material_tests PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/SandBox/assets/shaders/")
target_include_directories(material_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(material_tests PRIVATE cxx_std_17)
add_test(NAME material_tests COMMAND material_tests)

# Editor logic that needs no UI: selection, the hierarchy rows, duplicate / reparent (keeps the world position) / gizmo
# write-back, Euler editing, viewport maths, the gizmo interface.
add_executable(editor_tests
    editor_tests.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/Scene.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/scene/TransformSystem.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/SceneEditing.cpp
    ${CMAKE_SOURCE_DIR}/DXEngine/src/editor/HierarchyModel.cpp)
target_include_directories(editor_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/DXEngine/src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src)
target_compile_features(editor_tests PRIVATE cxx_std_17)
add_test(NAME editor_tests COMMAND editor_tests)
