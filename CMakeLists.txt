cmake_minimum_required(VERSION 2.8)

project(os-find)
add_executable(${PROJECT_NAME} "main.cpp" "exec_utils.hpp")

set(CMAKE_CXX_FLAGS_DEBUG "-fsanitize=address,undefined")

find_package(Boost REQUIRED COMPONENTS program_options)
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
