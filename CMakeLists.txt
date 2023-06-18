cmake_minimum_required(VERSION 3.21)
project(crescent)
include(.config/toolchain.cmake)
include(CMakeOptions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_STANDARD 23)
#set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
set(FILES
        ${PROJECT_SOURCE_DIR}/fonts/Tamsyn8x16r.psf
        ${PROJECT_SOURCE_DIR}/limine.cfg
        basic
        user_tty
        )

add_executable(crescent src/main.c)
target_include_directories(crescent PRIVATE src thirdparty include)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.config")

add_executable(basic tests/basic.c tests/basic.asm)
add_executable(user_tty tests/user_tty.c)
target_include_directories(user_tty PRIVATE include)

add_dependencies(crescent basic user_tty)

target_compile_options(crescent PRIVATE
        $<$<COMPILE_LANGUAGE:C>: -Wall
        -mgeneral-regs-only -mno-red-zone -mcmodel=kernel
        -masm=intel -fno-stack-protector -fno-exceptions -fno-rtti
        -fno-omit-frame-pointer -Wno-interrupt-service-routine
        -fPIC -nostdlibinc -Wno-error=unused>)
target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/.config/kernel.ld)

find_program(XORRISO xorriso REQUIRED)
find_program(CP cp REQUIRED)
find_program(GIT git REQUIRED)
find_program(MKDIR mkdir REQUIRED)

add_custom_command(TARGET crescent POST_BUILD
        COMMAND ${MKDIR} -p ${PROJECT_BINARY_DIR}/iso_root
        COMMAND ${CP} crescent ${FILES} ${PROJECT_BINARY_DIR}/iso_root
        COMMAND ${XORRISO} -as mkisofs
        -b limine-cd.bin -no-emul-boot
        -boot-load-size 4 -boot-info-table
        --efi-boot limine-cd-efi.bin
        --efi-boot-part --efi-boot-image
        --protective-msdos-label
        ${PROJECT_BINARY_DIR}/iso_root
        -o ${PROJECT_BINARY_DIR}/image.iso
        -quiet &> /dev/null VERBATIM USES_TERMINAL)

include(.config/utils.cmake)

add_subdirectory(src)
