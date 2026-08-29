cmake_minimum_required(VERSION 3.24)

project(gsrecon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    gsrecon
    WIN32
    src/main.cpp
)

target_compile_definitions(
    gsrecon
    PRIVATE
    UNICODE
    _UNICODE
)