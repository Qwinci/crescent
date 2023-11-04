cmake_minimum_required(VERSION 3.21)
project(crescent)

add_subdirectory(kernel)
add_subdirectory(apps)

include(image.cmake)
