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
