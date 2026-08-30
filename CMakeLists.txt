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
)

target_compile_definitions(
    gsrecon
    PRIVATE
    UNICODE
    _UNICODE
    VK_USE_PLATFORM_WIN32_KHR
    $<$<CONFIG:Debug>:GSRECON_ENABLE_VALIDATION>
)

target_link_libraries(
    gsrecon
    PRIVATE
    Vulkan::Headers
    Vulkan::Loader
)