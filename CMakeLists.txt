cmake_minimum_required(VERSION 3.21)
project(crescent)
include(.config/toolchain.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_STANDARD 23)
#set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)
set(FILES
        ${PROJECT_SOURCE_DIR}/fonts/Tamsyn8x16r.psf
        ${PROJECT_SOURCE_DIR}/limine.cfg
		crescent
        user_tty
        )

add_executable(crescent src/main.c)
target_include_directories(crescent PRIVATE src thirdparty include)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.config")

add_executable(user_tty tests/user_tty.c)
target_include_directories(user_tty PRIVATE include)

target_compile_options(crescent PRIVATE
        $<$<COMPILE_LANGUAGE:C>: -Wall
        -mgeneral-regs-only -mno-red-zone -mcmodel=kernel
        -fno-stack-protector -fno-exceptions -fno-rtti
        -fno-omit-frame-pointer
        -fPIC -nostdlibinc>)
target_compile_options(crescent PRIVATE $<$<COMPILE_LANGUAGE:ASM>: -Wno-unused-command-line-argument>)
target_link_options(crescent PRIVATE -T ${PROJECT_SOURCE_DIR}/.config/kernel.ld)

find_program(XORRISO xorriso REQUIRED)
find_program(CP cp REQUIRED)
find_program(GIT git REQUIRED)
find_program(MKDIR mkdir REQUIRED)

add_custom_command(OUTPUT image.iso DEPENDS ${FILES}
        COMMAND ${MKDIR} -p ${PROJECT_BINARY_DIR}/iso_root
        COMMAND ${CP} ${FILES} ${PROJECT_BINARY_DIR}/iso_root
        COMMAND ${XORRISO} -as mkisofs
        -b limine-bios-cd.bin -no-emul-boot
        -boot-load-size 4 -boot-info-table
        --efi-boot limine-uefi-cd.bin
        --efi-boot-part --efi-boot-image
        --protective-msdos-label
        ${PROJECT_BINARY_DIR}/iso_root
        -o ${PROJECT_BINARY_DIR}/image.iso
        -quiet &> /dev/null VERBATIM USES_TERMINAL)

include(.config/utils.cmake)

include(CMakeOptions.cmake)
add_subdirectory(src)
