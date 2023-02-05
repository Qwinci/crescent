cmake_minimum_required(VERSION 3.21)
project(crescent)
include(.config/toolchain.cmake)
include(CMakeOptions.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS True)
set(CMAKE_C_STANDARD 23)
set(CMAKE_CXX_STANDARD 20)
set(FILES
        ${PROJECT_SOURCE_DIR}/fonts/Tamsyn8x16r.psf
        ${PROJECT_SOURCE_DIR}/limine.cfg
        ${PROJECT_BINARY_DIR}/usertest)
set(QEMU_FLAGS -m 2G -machine q35 -smp 2
        -d int -no-reboot -no-shutdown -cpu qemu64,+invtsc
        -drive file=nvm.img,if=none,id=nvm,format=raw
        -device nvme,serial=deadbeef,drive=nvm -M smm=off
        -net nic,model=rtl8139)

file(GLOB_RECURSE SRC CONFIGURE_DEPENDS src/*.cpp src/*.asm)

add_executable(usertest tests/main.cpp tests/main.asm)
target_link_options(usertest PRIVATE -nostdlib -static -static-pie)

add_executable(crescent ${SRC} thirdparty/tinyvmem/vmem.cpp)
add_dependencies(crescent usertest)
target_include_directories(crescent PRIVATE src thirdparty)

target_compile_options(crescent PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>: -Wall
        -mno-80387 -mno-mmx -mno-3dnow -mno-sse -mno-sse2
        -mgeneral-regs-only -mno-red-zone -mcmodel=kernel
        -masm=intel -fno-stack-protector -fno-exceptions -fno-rtti
        -fno-omit-frame-pointer -Wno-interrupt-service-routine
        -fPIC -nostdlibinc>)
target_link_options(crescent PRIVATE
        -T ${PROJECT_SOURCE_DIR}/.config/kernel.ld
        -static -static-pie -nostdlib)

find_program(XORRISO xorriso REQUIRED)
find_program(CP cp REQUIRED)
find_program(GIT git REQUIRED)
find_program(MAKE make REQUIRED)
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

if (NOT EXISTS ${PROJECT_BINARY_DIR}/limine)
    execute_process(
            COMMAND ${GIT} clone https://github.com/limine-bootloader/limine.git
            -b v4.x-branch-binary --depth=1 ${PROJECT_BINARY_DIR}/limine
    )
    execute_process(COMMAND ${MAKE} -C ${PROJECT_BINARY_DIR}/limine)
    execute_process(COMMAND ${MKDIR} -p ${PROJECT_BINARY_DIR}/iso_root)
    execute_process(COMMAND ${CP}
            ${PROJECT_BINARY_DIR}/limine/limine.sys
            ${PROJECT_BINARY_DIR}/limine/limine-cd.bin
            ${PROJECT_BINARY_DIR}/limine/limine-cd-efi.bin
            ${PROJECT_BINARY_DIR}/iso_root)
endif ()

if (NOT EXISTS ${PROJECT_BINARY_DIR}/nvm.img)
    find_program(DD dd REQUIRED)
    execute_process(COMMAND ${DD} if=/dev/zero of=${PROJECT_BINARY_DIR}/nvm.img bs=1M count=50
            OUTPUT_QUIET ERROR_QUIET)
endif ()

add_custom_target(run_kvm
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -enable-kvm
        DEPENDS crescent USES_TERMINAL VERBATIM)

add_custom_target(run
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS}
        DEPENDS crescent USES_TERMINAL VERBATIM)

add_custom_target(debug
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -S -s
        DEPENDS crescent USES_TERMINAL VERBATIM)

add_custom_target(debug_kvm
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -enable-kvm -S -s
        DEPENDS crescent USES_TERMINAL VERBATIM)