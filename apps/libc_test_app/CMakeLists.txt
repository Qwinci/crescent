project(libc_test_app)

set(CMAKE_C_STANDARD 17)

add_executable(libc_test_app libc_test_app.c)

target_compile_options(libc_test_app PRIVATE --sysroot=${CMAKE_BINARY_DIR}/sysroot)
target_link_options(libc_test_app PRIVATE --sysroot=${CMAKE_BINARY_DIR}/sysroot -unwindlib=libunwind -Wl,-dynamic-linker,/lib/ld.so)
add_dependencies(libc_test_app mlibc)
