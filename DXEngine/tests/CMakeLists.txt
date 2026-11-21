# Engine tests. Enable with:  cmake -DDXENGINE_BUILD_TESTS=ON ...      Run:  ctest --output-on-failure
#
# Every test is ONE source file linked against DXEngineCore (the whole engine as a static library, see DXEngine/CMakeLists.txt).
# No engine source is listed here: a linker takes from a static library only the files a test actually reaches, so a test
# that needs just Scene and Mesh pulls in just those, and adding or moving an engine file touches no test target.
# What the core brings to every test: the include paths (src, entt, imgui, assimp, stb, tinyexr) and the libraries
# (ImGui, ImGuizmo, assimp, meshoptimizer, the D3D11 libraries).
#
# The test executables are named after their source file; the tests that need no GPU run everywhere, the ones labelled `gpu`
# need a D3D11 device and print SKIPPED (and pass) without one:  ctest -L gpu   /   ctest -LE gpu

function(dx_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE DXEngineCore)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

# ---- no GPU, no window --------------------------------------------------------------------------------------------------
dx_add_test(scene_tests)               # entities, hierarchy links, transforms (TransformSystem)
dx_add_test(render_tests)              # scene extraction: culling, routing, sorting, materials (meshes are made without a device)
dx_add_test(light_tests)               # Scene lights -> the GPU light data
dx_add_test(cluster_tests)             # clustered light assignment: grid maths, light -> cluster, the shader's pixel -> cluster formula
dx_add_test(rhi_tests)                 # RHI rules that need no device (buffer description validation)
dx_add_test(shadow_tests)              # cascade fitting, spot / point views, the atlas allocator, which lights get shadows
dx_add_test(material_tests)            # texture slot table, Material's derived texture mask, colour-space aware mips
target_compile_definitions(material_tests PRIVATE SHADER_DIR="${CMAKE_SOURCE_DIR}/Editor/assets/shaders/")
dx_add_test(editor_tests)              # selection, hierarchy rows, duplicate / reparent / gizmo write-back, picking, ...
dx_add_test(editor_ui_tests)           # the editor panels through real ImGui frames (ImGui's assertions are the oracle);
                                       # two renderer-side functions are stubbed in the test, so the renderer is never linked
dx_add_test(mesh_optimizer_tests)      # the wrapper around meshoptimizer, against the real library
dx_add_test(gpu_timings_tests)         # the GPU profiler's bookkeeping (header only)
dx_add_test(asset_tests)               # ModelAsset / Instantiate / Animator (meshes are made without a device)
dx_add_test(asset_catalog_tests)       # which model files lie in the assets folder
dx_add_test(skeleton_tests)            # the skeleton evaluator, transforms above a root bone, bounding box of a skinned mesh
dx_add_test(image_decode_tests)        # image files -> 8-bit pixels (stb_image, Radiance HDR, OpenEXR through tinyexr)
target_compile_definitions(image_decode_tests PRIVATE
    DX_TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/DXEngine/tests/data" DX_ASSETS_DIR="${CMAKE_SOURCE_DIR}/Editor/assets")

# ---- need a real D3D11 device (run by hand after touching src/RHI, the profiler or the importer) ------------------------
dx_add_test(d3d11_device_tests)        # queries, fences, V-Sync, the GPU profiler end to end
dx_add_test(model_loader_tests)        # the importer on the REAL sample models (assimp, textures, GPU meshes), then Instantiate + Animator
target_compile_definitions(model_loader_tests PRIVATE DX_ASSETS_DIR="${CMAKE_SOURCE_DIR}/Editor/assets")
set_tests_properties(d3d11_device_tests model_loader_tests PROPERTIES LABELS gpu)
