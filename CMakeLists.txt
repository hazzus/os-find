cmake_minimum_required(VERSION 2.8)

project(os-find)
add_executable(${PROJECT_NAME} "main.cpp" "exec_utils.hpp" "parse_utils.hpp")

set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address,undefined")
set(CMAKE_CXX_FLAGS_RELEASE "-O2")

