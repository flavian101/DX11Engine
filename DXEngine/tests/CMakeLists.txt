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
    ${CMAKE_SOURCE_DIR}/DXEngine/src/utils/material/Material.cpp)
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

# RHI rules that need no device (buffer description validation).
add_executable(rhi_tests rhi_tests.cpp)
target_include_directories(rhi_tests PRIVATE ${CMAKE_SOURCE_DIR}/DXEngine/src)
target_compile_features(rhi_tests PRIVATE cxx_std_17)
add_test(NAME rhi_tests COMMAND rhi_tests)
