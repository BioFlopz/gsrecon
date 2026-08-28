cmake_minimum_required(VERSION 3.24)

project(gsrecon LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(
    gsrecon
    src/main.cpp
)