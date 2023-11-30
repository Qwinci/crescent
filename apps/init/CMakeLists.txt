project(init)

set(CMAKE_C_STANDARD 23)

add_executable(init init.c)

target_link_libraries(init PRIVATE common)
