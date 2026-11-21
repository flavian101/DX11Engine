project(Editor)

file(GLOB_RECURSE EDITOR_SRC
	"src/*.cpp"
	"src/*.h"
)

add_executable(Editor ${EDITOR_SRC})

# Copy assets next to the built executable (multi-config safe). The folder is hundreds of MB of textures and models.
# CMake >= 3.26 can copy only what changed, so the copy is its own target that runs on every build (a shader edit is
# picked up without relinking the Editor, and an unchanged tree costs a directory scan). Older CMake can only copy
# everything, so there it stays a POST_BUILD step (it then runs only when the Editor is relinked).
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    add_custom_target(EditorAssets ALL
        COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/Editor/assets"
            "$<TARGET_FILE_DIR:Editor>/assets"
        COMMENT "Copying editor assets")
    add_dependencies(Editor EditorAssets)
    set_target_properties(EditorAssets PROPERTIES FOLDER "Editor")
else()
    add_custom_command(TARGET Editor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/Editor/assets"
            "$<TARGET_FILE_DIR:Editor>/assets"
    )
endif()

# Make Visual Studio debugger use the exe folder as working directory
set_target_properties(Editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Editor>"
)

target_include_directories(Editor PUBLIC
	${CMAKE_SOURCE_DIR}/DXEngine/src
)

target_link_libraries(Editor PRIVATE DXEngineEditor)

target_compile_definitions(Editor PUBLIC
    UNICODE
    _UNICODE
)