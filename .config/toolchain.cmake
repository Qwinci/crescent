set(CMAKE_SYSTEM_NAME Generic)

set(triple x86_64-unknown-none-elf)
set(CMAKE_C_COMPILER clang CACHE STRING "" FORCE)
set(CMAKE_C_COMPILER_TARGET ${triple})
set(CMAKE_CXX_COMPILER clang++ CACHE STRING "" FORCE)
set(CMAKE_CXX_COMPILER_TARGET ${triple})
add_link_options(-fuse-ld=lld)

enable_language(C CXX ASM_NASM)
set(CMAKE_ASM_NASM_OBJECT_FORMAT elf64)