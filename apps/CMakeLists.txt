cmake_minimum_required(VERSION 3.19)
project(crescent_apps)

set(CMAKE_C_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory(init)
add_subdirectory(user_tty)
