cmake_minimum_required(VERSION 3.5.0)
project(webgpu-rtr VERSION 0.1.0 LANGUAGES C CXX)

include(CTest)
enable_testing()

add_subdirectory(external/glfw)
add_subdirectory(external/webgpu)
add_subdirectory(external/glfw3webgpu)
add_subdirectory(external/imgui)

add_executable(
    webgpu-rtr 
    src/main.cpp
    src/implementations.cpp
    src/core/Renderer.h
    src/core/Renderer.cpp
    src/core/Engine.h
    src/core/Engine.cpp
    src/resource/ResourceManager.h
    src/resource/ResourceManager.cpp
)

# Add some include paths
target_include_directories(webgpu-rtr PRIVATE "src")
target_include_directories(webgpu-rtr PRIVATE "external")

target_compile_options(webgpu-rtr PRIVATE -Wall -Wextra -pedantic)
target_link_libraries(webgpu-rtr PRIVATE glfw webgpu glfw3webgpu imgui)
set_target_properties(webgpu-rtr PROPERTIES
	CXX_STANDARD 17
	VS_DEBUGGER_ENVIRONMENT "DAWN_DEBUG_BREAK_ON_ERROR=1"
)
target_compile_definitions(webgpu-rtr PRIVATE
    ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)


# Should not be needed for Dawn
target_copy_webgpu_binaries(webgpu-rtr)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
