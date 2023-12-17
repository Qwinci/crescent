project(init)

set(CMAKE_C_STANDARD 23)

add_executable(init init.c)
App(init)

target_link_libraries(init PRIVATE common)
