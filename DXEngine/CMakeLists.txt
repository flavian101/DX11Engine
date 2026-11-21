project(DXEngine)

# Two static libraries, both from one source tree (every file under src/, found by the globs below: adding or moving a file
# needs no list to be edited).
#
#   DXEngineCore    everything except src/editor/: the engine a game links. No ImGui, no ImGuizmo, no file dialogs.
#                   It has no entry point (Main.h is a header the application includes).
#   DXEngineEditor  src/editor/: the panels, the editor logic and the ImGui layer. Links the core + ImGui + ImGuizmo.
#
# 'DXEngine' stays as an alias of the core, for applications that are not the editor.
file(GLOB_RECURSE DXENGINE_ALL_SRC
	"src/*.cpp"
	"src/*.h"
)
file(GLOB_RECURSE DXENGINE_EDITOR_SRC
	"src/editor/*.cpp"
	"src/editor/*.h"
)
set(DXENGINE_CORE_SRC ${DXENGINE_ALL_SRC})
list(REMOVE_ITEM DXENGINE_CORE_SRC ${DXENGINE_EDITOR_SRC})

# ---- core ----------------------------------------------------------------------------------------------------------------
add_library(DXEngineCore STATIC ${DXENGINE_CORE_SRC})
add_library(DXEngine ALIAS DXEngineCore)

target_compile_definitions(DXEngineCore PUBLIC
    UNICODE
    _UNICODE
	_CRT_SECURE_NO_WARNINGS
)

target_include_directories(DXEngineCore PUBLIC
    src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/assimp/include
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/stb
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/tinyexr
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/entt/src
)

target_link_libraries(DXEngineCore
	PUBLIC
	  assimp
	  meshoptimizer
)

# The D3D11 backend and the texture loader need these. Visual Studio takes them from the #pragma comment(lib) lines in
# dxpch.h; naming them here makes every linker (and every target that links the core) get them.
if(WIN32)
	target_link_libraries(DXEngineCore PUBLIC d3d11 dxgi d3dcompiler dxguid ole32)
endif()

# ---- editor ---------------------------------------------------------------------------------------------------------------
add_library(DXEngineEditor STATIC ${DXENGINE_EDITOR_SRC})

target_include_directories(DXEngineEditor PUBLIC
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/imgui
)

target_link_libraries(DXEngineEditor
	PUBLIC
	  DXEngineCore
	  ImGui
	  ImGuizmo
	  comdlg32
)
