project(Editor)

file(GLOB_RECURSE EDITOR_SRC
	"src/*.cpp"
	"src/*.h"
)

add_executable(Editor ${EDITOR_SRC})

# Copy assets next to the built executable (multi-config safe)
add_custom_command(TARGET Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
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