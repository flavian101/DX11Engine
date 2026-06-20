cmake_minimum_required(VERSION 3.20)

project (DX11Engine)

#Enable folders in visual studio
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

#set the c++ standard globally
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#Build static libraries by default
set(BUILD_SHARED_LIBS OFF)

#Add vendor libraries
add_subdirectory(DXEngine/vendor/assimp)
add_subdirectory(DXEngine/vendor/imgui)


#Add engine and sandbox
add_subdirectory(DXEngine)
add_subdirectory(Sandbox)




