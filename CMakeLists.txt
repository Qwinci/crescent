cmake_minimum_required(VERSION 3.21)
project(crescent)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

add_subdirectory(kernel)
add_subdirectory(libs)
add_subdirectory(apps)

include(image.cmake)
