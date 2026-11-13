# Dear ImGui, DOCKING branch (the submodule follows the 'docking' branch of the fork, see .gitmodules).
#
# The fork has no CMakeLists.txt (it ships ImGui.vcxproj for Visual Studio), so the library is defined here.
# The platform + renderer backends are part of the library, so both build systems compile the same sources:
# the Visual Studio project compiles them inside DXEngine.vcxproj (ImGui.vcxproj only has the core files).
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/DXEngine/vendor/imgui)

if(EXISTS ${IMGUI_DIR}/CMakeLists.txt)
    add_subdirectory(${IMGUI_DIR})
else()
    add_library(ImGui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/imgui_demo.cpp)
    target_include_directories(ImGui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    if(WIN32)
        target_sources(ImGui PRIVATE
            ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
            ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp)
        target_link_libraries(ImGui PUBLIC d3d11 d3dcompiler dxgi dwmapi)
    endif()
endif()
