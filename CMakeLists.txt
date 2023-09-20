cmake_minimum_required(VERSION 3.27.0)
project(procplanets VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_subdirectory(external/glfw)
add_subdirectory(external/webgpu)
add_subdirectory(external/glfw3webgpu)
add_subdirectory(external/imgui)

add_executable(
    procplanets 
    src/main.cpp
    src/implementations.cpp
    src/core/Renderer.h
    src/core/Renderer.cpp
    src/core/Engine.h
    src/core/Engine.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
    src/procgen/PlanetGenerator.h
    src/procgen/PlanetGenerator.cpp
    src/procgen/FastNoiseLite.h
    src/procgen/ElevationGenerator.hpp
)

# Add some include paths
target_include_directories(procplanets PRIVATE "src")
target_include_directories(procplanets PRIVATE "external")

target_compile_options(procplanets PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(procplanets PRIVATE glfw webgpu glfw3webgpu imgui)
set_target_properties(procplanets PROPERTIES
	CXX_STANDARD 17
	VS_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)
target_compile_definitions(procplanets PRIVATE
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)


# Should not be needed for Dawn
target_copy_webgpu_binaries(procplanets)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
