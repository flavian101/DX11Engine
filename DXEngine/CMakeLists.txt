project(DXEngine)

file(GLOB_RECURSE DXENGINE_SRC
	"src/*.cpp"
	"src/*.h"
)


# DXEngineCore: the whole engine as one static library (every file under src/, found by the glob above: adding or moving a
# file needs no list to be edited). It has no entry point (Main.h is a header the application includes), so the Editor
# and every test link it. 'DXEngine' stays as an alias for the name the Editor already uses.
add_library(DXEngineCore STATIC ${DXENGINE_SRC})
add_library(DXEngine ALIAS DXEngineCore)

target_compile_definitions(DXEngineCore PUBLIC
    UNICODE
    _UNICODE
)

target_include_directories(DXEngineCore PUBLIC
    src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/imgui
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/assimp/include
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/stb
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/tinyexr
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src
)


target_link_libraries(DXEngineCore
	PUBLIC
	  ImGui
	  ImGuizmo
	  assimp
	  meshoptimizer
	  comdlg32
)

# The D3D11 backend and the texture loader need these. Visual Studio takes them from the #pragma comment(lib) lines in
# dxpch.h; naming them here makes every linker (and every target that links the core) get them.
if(WIN32)
	target_link_libraries(DXEngineCore PUBLIC d3d11 dxgi d3dcompiler dxguid ole32)
endif()

target_compile_definitions(DXEngineCore PUBLIC
	_CRT_SECURE_NO_WARNINGS
)

