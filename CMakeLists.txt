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

# Engine tests (no GPU needed):  cmake -DDXENGINE_BUILD_TESTS=ON ..  then  ctest --output-on-failure
option(DXENGINE_BUILD_TESTS "Build the engine tests" OFF)
if(DXENGINE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(DXEngine/tests)
endif()




