cmake_minimum_required(VERSION 3.24)

project(gsrecon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#
# Project-local tool manifest
#

set(
    GSRECON_TOOL_VERSIONS
    "${CMAKE_SOURCE_DIR}/tools/tool-versions.json"
)

file(
    READ
    "${GSRECON_TOOL_VERSIONS}"
    GSRECON_TOOL_VERSIONS_JSON
)


#
# Project-local Vulkan SDK tools
#

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
        "Run tools/bootstrap-vulkan-tools.ps1 first."
    )
endif()


#
# Project-local CUDA Runtime
#

string(
    JSON
    GSRECON_CUDA_RUNTIME_DIRECTORY
    GET
    "${GSRECON_TOOL_VERSIONS_JSON}"
    cudaRuntime
    installDirectory
)

set(
    GSRECON_CUDA_RUNTIME_ROOT
    "${CMAKE_SOURCE_DIR}/${GSRECON_CUDA_RUNTIME_DIRECTORY}"
)

set(
    GSRECON_CUDA_INCLUDE_DIR
    "${GSRECON_CUDA_RUNTIME_ROOT}/include"
)

set(
    GSRECON_CUDART_LIBRARY
    "${GSRECON_CUDA_RUNTIME_ROOT}/lib/x64/cudart.lib"
)

set(
    GSRECON_CUDART_DLL
    "${GSRECON_CUDA_RUNTIME_ROOT}/bin/x64/cudart64_13.dll"
)

if(NOT EXISTS "${GSRECON_CUDA_INCLUDE_DIR}/cuda_runtime_api.h")
    message(
        FATAL_ERROR
        "CUDA Runtime headers not found. "
        "Run tools/bootstrap-cuda.ps1 first."
    )
endif()

if(NOT EXISTS "${GSRECON_CUDART_LIBRARY}")
    message(
        FATAL_ERROR
        "CUDA Runtime library not found. "
        "Run tools/bootstrap-cuda.ps1 first."
    )
endif()

if(NOT EXISTS "${GSRECON_CUDART_DLL}")
    message(
        FATAL_ERROR
        "CUDA Runtime DLL not found. "
        "Run tools/bootstrap-cuda.ps1 first."
    )
endif()


#
# Project-local CUDA CRT
#

string(
    JSON
    GSRECON_CUDA_CRT_DIRECTORY
    GET
    "${GSRECON_TOOL_VERSIONS_JSON}"
    cudaCrt
    installDirectory
)

set(
    GSRECON_CUDA_CRT_ROOT
    "${CMAKE_SOURCE_DIR}/${GSRECON_CUDA_CRT_DIRECTORY}"
)

set(
    GSRECON_CUDA_CRT_INCLUDE_DIR
    "${GSRECON_CUDA_CRT_ROOT}/include"
)

if(NOT EXISTS "${GSRECON_CUDA_CRT_INCLUDE_DIR}/crt/host_defines.h")
    message(
        FATAL_ERROR
        "CUDA CRT headers not found. "
        "Run tools/bootstrap-cuda.ps1 first."
    )
endif()


#
# Project-local CUDA NVCC
#

string(
    JSON
    GSRECON_CUDA_NVCC_DIRECTORY
    GET
    "${GSRECON_TOOL_VERSIONS_JSON}"
    cudaNvcc
    installDirectory
)

set(
    GSRECON_NVCC
    "${CMAKE_SOURCE_DIR}/${GSRECON_CUDA_NVCC_DIRECTORY}/bin/nvcc.exe"
)

if(NOT EXISTS "${GSRECON_NVCC}")
    message(
        FATAL_ERROR
        "CUDA NVCC not found. "
        "Run tools/bootstrap-cuda.ps1 first."
    )
endif()


#
# Vulkan source dependencies
#

add_subdirectory(3rdparty/Vulkan-Headers)
add_subdirectory(3rdparty/Vulkan-Loader)


#
# CUDA kernel
#

set(
    GSRECON_CUDA_KERNEL
    "${CMAKE_SOURCE_DIR}/src/cuda_kernel.cu"
)

set(
    GSRECON_CUDA_KERNEL_OBJECT
    "${CMAKE_BINARY_DIR}/cuda/$<CONFIG>/cuda_kernel.obj"
)

add_custom_command(
    OUTPUT
        "${GSRECON_CUDA_KERNEL_OBJECT}"

    COMMAND
        "${CMAKE_COMMAND}"
        -E
        make_directory
        "${CMAKE_BINARY_DIR}/cuda/$<CONFIG>"

    COMMAND
        "${GSRECON_NVCC}"
        --std=c++20
        --gpu-architecture=sm_75
        "--compiler-options=$<IF:$<CONFIG:Debug>,/MDd,/MD>"
        --compile
        --include-path "${GSRECON_CUDA_INCLUDE_DIR}"
        --include-path "${GSRECON_CUDA_CRT_INCLUDE_DIR}"
        "${GSRECON_CUDA_KERNEL}"
        -o "${GSRECON_CUDA_KERNEL_OBJECT}"

    DEPENDS
        "${GSRECON_CUDA_KERNEL}"
        "${CMAKE_SOURCE_DIR}/src/gaussian.hpp"

    COMMENT
        "Compiling cuda_kernel.cu"

    VERBATIM
)

set_source_files_properties(
    "${GSRECON_CUDA_KERNEL_OBJECT}"
    PROPERTIES
        EXTERNAL_OBJECT TRUE
)


#
# Application
#

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

    "${GSRECON_CUDA_KERNEL_OBJECT}"
)


#
# CUDA headers
#

target_include_directories(
    gsrecon
    PRIVATE
        "${GSRECON_CUDA_INCLUDE_DIR}"
        "${GSRECON_CUDA_CRT_INCLUDE_DIR}"
)


#
# HLSL compilation helper
#

function(gsrecon_compile_hlsl output source profile entry_point)
    add_custom_command(
        OUTPUT
            "${output}"

        COMMAND
            "${CMAKE_COMMAND}"
            -E
            make_directory
            "${CMAKE_BINARY_DIR}/shaders"

        COMMAND
            "${GSRECON_DXC}"
            -spirv
            -T "${profile}"
            -E "${entry_point}"
            "${source}"
            -Fo "${output}"

        DEPENDS
            "${source}"

        COMMENT
            "Compiling ${source}"

        VERBATIM
    )
endfunction()



#
# Gaussian contract shader
#

set(
    GSRECON_GAUSSIAN_CONTRACT_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/gaussian_contract.hlsl"
)

set(
    GSRECON_GAUSSIAN_CONTRACT_SPV
    "${CMAKE_BINARY_DIR}/shaders/gaussian_contract.spv"
)

gsrecon_compile_hlsl(
    "${GSRECON_GAUSSIAN_CONTRACT_SPV}"
    "${GSRECON_GAUSSIAN_CONTRACT_SHADER}"
    "cs_6_0"
    "main"
)


#
# Gaussian vertex shader
#

set(
    GSRECON_GAUSSIAN_VERTEX_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/gaussian.vert.hlsl"
)

set(
    GSRECON_GAUSSIAN_VERTEX_SPV
    "${CMAKE_BINARY_DIR}/shaders/gaussian.vert.spv"
)

gsrecon_compile_hlsl(
    "${GSRECON_GAUSSIAN_VERTEX_SPV}"
    "${GSRECON_GAUSSIAN_VERTEX_SHADER}"
    "vs_6_0"
    "main"
)


#
# Gaussian fragment shader
#

set(
    GSRECON_GAUSSIAN_FRAGMENT_SHADER
    "${CMAKE_SOURCE_DIR}/shaders/gaussian.frag.hlsl"
)

set(
    GSRECON_GAUSSIAN_FRAGMENT_SPV
    "${CMAKE_BINARY_DIR}/shaders/gaussian.frag.spv"
)

gsrecon_compile_hlsl(
    "${GSRECON_GAUSSIAN_FRAGMENT_SPV}"
    "${GSRECON_GAUSSIAN_FRAGMENT_SHADER}"
    "ps_6_0"
    "main"
)


#
# Shader target
#

add_custom_target(
    gsrecon_shaders
    DEPENDS
        "${GSRECON_GAUSSIAN_CONTRACT_SPV}"
        "${GSRECON_GAUSSIAN_VERTEX_SPV}"
        "${GSRECON_GAUSSIAN_FRAGMENT_SPV}"
)

add_dependencies(
    gsrecon
    gsrecon_shaders
)


#
# Compile definitions
#

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


#
# Libraries
#

target_link_libraries(
    gsrecon
    PRIVATE
        Vulkan::Headers
        Vulkan::Loader
        "${GSRECON_CUDART_LIBRARY}"
)


#
# Copy project-local CUDA Runtime beside gsrecon.exe
#

add_custom_command(
    TARGET
        gsrecon

    POST_BUILD

    COMMAND
        "${CMAKE_COMMAND}"
        -E
        copy_if_different
        "${GSRECON_CUDART_DLL}"
        "$<TARGET_FILE_DIR:gsrecon>"

    VERBATIM
)