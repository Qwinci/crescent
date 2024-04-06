set(CMAKE_SYSTEM_NAME Generic)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_C_FLAGS "-mno-outline-atomics")
set(CMAKE_CXX_FLAGS "-mno-outline-atomics")
set(CMAKE_C_COMPILER_TARGET "aarch64-none-elf")
set(CMAKE_CXX_COMPILER_TARGET "aarch64-none-elf")
set(CMAKE_ASM_COMPILER_TARGET "aarch64-none-elf")
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(ARCH "aarch64")
