project(Editor)

file(GLOB_RECURSE EDITOR_SRC
	"src/*.cpp"
	"src/*.h"
)

add_executable(Editor ${EDITOR_SRC})

# Copy assets next to the built executable (multi-config safe). The folder is hundreds of MB of textures and models:
# copy only what changed (CMake 3.26+), otherwise every build would rewrite all of it.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(EDITOR_COPY_ASSETS_CMD copy_directory_if_different)
else()
    set(EDITOR_COPY_ASSETS_CMD copy_directory)
endif()
add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E ${EDITOR_COPY_ASSETS_CMD}
        "${CMAKE_SOURCE_DIR}/Editor/assets"
        "$<TARGET_FILE_DIR:Editor>/assets"
)

# Make Visual Studio debugger use the exe folder as working directory
set_target_properties(Editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Editor>"
)

target_include_directories(Editor PUBLIC
	${CMAKE_SOURCE_DIR}/DXEngine/src
)

target_link_libraries(Editor PRIVATE DXEngine)

target_compile_definitions(Editor PUBLIC
    UNICODE
    _UNICODE
)