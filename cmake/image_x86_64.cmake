set(FILES
	${PROJECT_SOURCE_DIR}/fonts/Tamsyn8x16r.psf
	${PROJECT_SOURCE_DIR}/limine.cfg
	bin/crescent
	initramfs.tar
)

set(QEMU_FLAGS -m 4G -M q35,smm=off -smp 1
	-no-shutdown -no-reboot -cpu qemu64,+umip,+smep,+smap
	#-d int
	#-device qemu-xhci
	-device virtio-net-pci-non-transitional
	#-device usb-kbd
	#-device usb-mouse
	-trace "*xhci*"
	-serial stdio
	#-device ich9-intel-hda,bus=pcie.0,addr=0x1B,debug=3
	#-device hda-output,audiodev=hda,debug=3
	#-audiodev pa,id=hda
	#-device vfio-pci,host=00:1f.3
)

find_program(XORRISO xorriso REQUIRED)

add_custom_command(OUTPUT image.iso DEPENDS ${FILES} crescent
	COMMAND cmake -E copy_if_different ${FILES} ${PROJECT_BINARY_DIR}/iso_root
	COMMAND ${XORRISO} -as mkisofs -b limine-bios-cd.bin -no-emul-boot
	-boot-load-size 4 -boot-info-table --efi-boot limine-uefi-cd.bin
	--efi-boot-part --efi-boot-image --protective-msdos-label
	${PROJECT_BINARY_DIR}/iso_root -o ${PROJECT_BINARY_DIR}/image.iso
	-quiet &> /dev/null VERBATIM USES_TERMINAL
)

if(NOT EXISTS "${CMAKE_BINARY_DIR}/limine")
	execute_process(
		COMMAND git clone https://github.com/limine-bootloader/limine
		-b v6.x-branch-binary --depth=1 "${CMAKE_BINARY_DIR}/limine"
	)
	execute_process(COMMAND mkdir -p ${PROJECT_BINARY_DIR}/iso_root)
	execute_process(COMMAND cmake -E copy_if_different
		${PROJECT_BINARY_DIR}/limine/limine-bios.sys
		${PROJECT_BINARY_DIR}/limine/limine-bios-cd.bin
		${PROJECT_BINARY_DIR}/limine/limine-uefi-cd.bin
		${PROJECT_BINARY_DIR}/iso_root
	)
endif()

add_custom_target(run
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso ${QEMU_FLAGS}
	DEPENDS image.iso initramfs.tar USES_TERMINAL VERBATIM
)

add_custom_target(run_kvm
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso ${QEMU_FLAGS} -enable-kvm
	DEPENDS image.iso initramfs.tar USES_TERMINAL VERBATIM
)

add_custom_target(debug
	COMMAND qemu-system-x86_64 -boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso -s -S ${QEMU_FLAGS}
	DEPENDS image.iso initramfs.tar USES_TERMINAL VERBATIM
)
