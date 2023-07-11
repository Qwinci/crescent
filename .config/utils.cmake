set(QEMU_FLAGS -m 8G -machine q35 -smp 1
        -d int -no-reboot -no-shutdown -cpu qemu64
        -drive file=nvm.img,if=none,id=nvm,format=raw
        -device nvme,serial=deadbeef,drive=nvm -M smm=off
        -trace pci_nvme_err*
        -device qemu-xhci
        -netdev user,id=mynet0 -device virtio-net-pci-non-transitional,netdev=mynet0
        )

if(NOT EXISTS ${PROJECT_BINARY_DIR}/limine)
    execute_process(
            COMMAND ${GIT} clone https://github.com/limine-bootloader/limine.git
            -b v5.x-branch-binary --depth=1 ${PROJECT_BINARY_DIR}/limine
    )
    execute_process(COMMAND ${MKDIR} -p ${PROJECT_BINARY_DIR}/iso_root)
    execute_process(COMMAND ${CP}
            ${PROJECT_BINARY_DIR}/limine/limine-bios.sys
            ${PROJECT_BINARY_DIR}/limine/limine-bios-cd.bin
            ${PROJECT_BINARY_DIR}/limine/limine-uefi-cd.bin
            ${PROJECT_BINARY_DIR}/iso_root)
endif()

if(NOT EXISTS ${PROJECT_BINARY_DIR}/nvm.img)
    find_program(DD dd REQUIRED)
    execute_process(COMMAND ${DD} if=/dev/zero of=${PROJECT_BINARY_DIR}/nvm.img bs=1M count=50
            OUTPUT_QUIET ERROR_QUIET)
endif()

add_custom_target(run_kvm
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -enable-kvm -cpu host,+invtsc
        DEPENDS image.iso USES_TERMINAL VERBATIM)

add_custom_target(run
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS}
        DEPENDS image.iso USES_TERMINAL VERBATIM)

add_custom_target(debug
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -S -s
        DEPENDS image.iso USES_TERMINAL VERBATIM)

add_custom_target(debug_kvm
        COMMAND qemu-system-x86_64
        -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
        ${QEMU_FLAGS} -enable-kvm -S -s
        DEPENDS image.iso USES_TERMINAL VERBATIM)