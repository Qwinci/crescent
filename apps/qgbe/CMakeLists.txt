cmake_minimum_required(VERSION 3.20)
project(qgbe LANGUAGES C VERSION 0.1)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_EXTENSIONS False)

add_executable(qgbe
        src/main.c
        src/utils/fsize.c
        src/emu.c
        src/bus.c
        src/cpu.c
        src/timer.c
        src/inst.c
        src/dis.c
        src/cpu_instrs.c
        src/ppu_utils.c
        src/ppu.c
        src/apu.c

        src/mbc/no_mbc.c
        src/mbc/mbc1.c
        src/mbc/mbc3.c)
target_compile_options(qgbe PRIVATE -Wall -Wextra)
target_include_directories(qgbe PRIVATE src)
target_compile_definitions(qgbe PRIVATE QGBE_VERSION="${PROJECT_VERSION}")
target_link_libraries(qgbe PRIVATE windower common)

install(TARGETS qgbe)
