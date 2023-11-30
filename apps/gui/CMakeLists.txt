project(gui)

set(CMAKE_C_STANDARD 23)

add_executable(gui main.c)

target_link_libraries(gui PRIVATE common)
