cmake_minimum_required(VERSION 3.21)
project(crescent CXX ASM)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_CXX_STANDARD 20)
set(FILES
        ${PROJECT_SOURCE_DIR}/fonts/Tamsyn8x16r.psf
        ${PROJECT_SOURCE_DIR}/limine.cfg
        )

add_executable(crescent src/main.cpp)
target_include_directories(crescent PRIVATE src thirdparty include src/std)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.config")

target_compile_options(crescent PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>: -Wall -Wextra -fno-stack-protector -fno-exceptions -fno-rtti
		-fno-omit-frame-pointer -fPIC -fsized-deallocation>)
target_compile_options(crescent PRIVATE -Wno-unused-command-line-argument)

target_link_options(crescent PRIVATE -Wl,--fatal-warnings)

find_program(XORRISO xorriso REQUIRED)
find_program(CP cp REQUIRED)
find_program(GIT git REQUIRED)
find_program(MKDIR mkdir REQUIRED)

add_custom_command(TARGET crescent POST_BUILD
        COMMAND ${MKDIR} -p ${PROJECT_BINARY_DIR}/iso_root
        COMMAND ${CP} crescent ${FILES} ${PROJECT_BINARY_DIR}/iso_root
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
include(.config/toolchain.cmake)

add_subdirectory(src)
