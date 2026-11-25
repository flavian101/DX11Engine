project(Game)

# A minimal game on the engine: links DXEngineCore only, so it carries no ImGui, no ImGuizmo and no editor code. It exists
# to keep that true (if the core grows a dependency on the editor, this target stops linking) and as the smallest example
# of an application: one Application subclass, one Layer.

add_executable(Game src/GameApp.cpp)

# The engine finds its shaders at assets/shaders/ under the project root: the nearest folder above the exe (or the working directory)
# that has assets/shaders (Main.h). Copy just the shaders next to the exe, which makes the exe's folder the root.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.26)
    set(GAME_COPY_CMD copy_directory_if_different)
else()
    set(GAME_COPY_CMD copy_directory)
endif()
add_custom_target(GameAssets ALL
    COMMAND ${CMAKE_COMMAND} -E ${GAME_COPY_CMD}
        "${CMAKE_SOURCE_DIR}/Editor/assets/shaders"
        "$<TARGET_FILE_DIR:Game>/assets/shaders"
    COMMENT "Copying shaders for Game")
add_dependencies(Game GameAssets)
set_target_properties(GameAssets PROPERTIES FOLDER "Game")

set_target_properties(Game PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:Game>")
target_link_libraries(Game PRIVATE DXEngineCore)
