set(CMAKE_SYSTEM_NAME Generic)

add_link_options(-static -static-pie -nostdlib -Wl,--fatal-warnings)

enable_language(C CXX ASM)
