set(CMAKE_SYSTEM_NAME Generic)

set(triple x86_64-unknown-linux-gnu)
set(CMAKE_C_COMPILER clang CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_TARGET ${triple})
add_link_options(-fuse-ld=lld -static -static-pie -nostdlib -Wl,--fatal-warnings)

enable_language(C CXX ASM)
