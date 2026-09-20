project(Sandbox)

file(GLOB_RECURSE SANDBOX_SRC
	"src/*.cpp"
	"src/*.h"
)

add_executable(Sandbox ${SANDBOX_SRC})

# Copy assets next to the built executable (multi-config safe)
add_custom_command(TARGET Sandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Sandbox/assets"
        "$<TARGET_FILE_DIR:Sandbox>/assets"
)

# Make Visual Studio debugger use the exe folder as working directory
set_target_properties(Sandbox PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Sandbox>"
)

target_include_directories(Sandbox PUBLIC
	${CMAKE_SOURCE_DIR}/DXEngine/src
)

target_link_libraries(Sandbox PRIVATE DXEngine)

target_compile_definitions(Sandbox PUBLIC
    UNICODE
    _UNICODE
)