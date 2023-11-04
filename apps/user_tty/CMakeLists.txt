project(user_tty)

set(CMAKE_C_STANDARD 23)

add_executable(user_tty user_tty.c)

target_include_directories(user_tty PRIVATE ${CMAKE_SOURCE_DIR}/kernel/include)
target_compile_options(user_tty PRIVATE -fno-stack-protector)
target_link_options(user_tty PRIVATE -nostdlib)
