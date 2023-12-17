project(gui)

set(CMAKE_C_STANDARD 23)

add_executable(gui main.c)
App(gui)

target_link_libraries(gui PRIVATE common)
