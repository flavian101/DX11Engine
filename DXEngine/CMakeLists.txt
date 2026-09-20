project(DXEngine)

file(GLOB_RECURSE DXENGINE_SRC
	"src/*.cpp"
	"src/*.h"
)


add_library(DXEngine STATIC ${DXENGINE_SRC})

target_compile_definitions(DXEngine PUBLIC
    UNICODE
    _UNICODE
)

target_include_directories(DXEngine PUBLIC 
    src
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/imgui
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/assimp/include
    ${CMAKE_SOURCE_DIR}/DXEngine/vendor/stb
)


target_link_libraries(DXEngine
	PUBLIC
	  ImGui
	  assimp
)

target_compile_definitions(DXEngine PUBLIC
	_CRT_SECURE_NO_WARNINGS
)

