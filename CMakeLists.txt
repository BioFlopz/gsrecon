cmake_minimum_required(VERSION 3.24)

project(gsrecon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(3rdparty/Vulkan-Headers)
add_subdirectory(3rdparty/Vulkan-Loader)

add_executable(
    gsrecon
    WIN32
    src/main.cpp
    src/vulkan_context.cpp
    src/vulkan_context.hpp
    src/vulkan_swapchain.cpp
    src/vulkan_swapchain.hpp
    src/vulkan_frame.cpp
    src/vulkan_frame.hpp
    src/shader.cpp
    src/shader.hpp
)

set(
    GSRECON_TOOL_VERSIONS
    "${CMAKE_SOURCE_DIR}/tools/capture/tool-versions.json"
)

file(
    READ
    "${GSRECON_TOOL_VERSIONS}"
    GSRECON_TOOL_VERSIONS_JSON
)

string(
    JSON
    GSRECON_VULKAN_TOOLS_DIRECTORY
    GET
    "${GSRECON_TOOL_VERSIONS_JSON}"
    vulkanSdkTools
    installDirectory
)

set(
    GSRECON_DXC
    "${CMAKE_SOURCE_DIR}/${GSRECON_VULKAN_TOOLS_DIRECTORY}/Bin/dxc.exe"
)

if(NOT EXISTS "${GSRECON_DXC}")
    message(
        FATAL_ERROR
        "DXC not found: ${GSRECON_DXC}\n"
        "Run tools/capture/bootstrap-vulkan-tools.ps1 first."
    )
endif()

set(
    GSRECON_COMPUTE_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/compute.hlsl"
)

set(
    GSRECON_COMPUTE_SPV
    "${CMAKE_BINARY_DIR}/shaders/compute.spv"
)

add_custom_command(
    OUTPUT "${GSRECON_COMPUTE_SPV}"

    COMMAND
        ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_BINARY_DIR}/shaders"

    COMMAND
        "${GSRECON_DXC}"
        -spirv
        -T cs_6_0
        -E main
        "${GSRECON_COMPUTE_SHADER}"
        -Fo "${GSRECON_COMPUTE_SPV}"

    DEPENDS
        "${GSRECON_COMPUTE_SHADER}"

    COMMENT "Compiling compute.hlsl"
    VERBATIM
)

add_custom_target(
    gsrecon_shaders
    DEPENDS "${GSRECON_COMPUTE_SPV}"
)

add_dependencies(
    gsrecon
    gsrecon_shaders
)

target_compile_definitions(
    gsrecon
    PRIVATE
    UNICODE
    _UNICODE
    VK_USE_PLATFORM_WIN32_KHR
    GSRECON_SHADER_DIR="${CMAKE_BINARY_DIR}/shaders"
    $<$<CONFIG:Debug>:GSRECON_ENABLE_VALIDATION>
    $<$<CONFIG:Debug>:GSRECON_ENABLE_DEBUG_CONSOLE>
)

target_link_libraries(
    gsrecon
    PRIVATE
    Vulkan::Headers
    Vulkan::Loader
)