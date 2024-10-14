set(QEMU_FLAGS -M virt,gic-version=3 -cpu cortex-a76
	-m 1G -smp 4 #-no-reboot -no-shutdown
	#-d int
	-serial stdio
)

add_custom_command(OUTPUT crescent.bin
	COMMAND ${CMAKE_OBJCOPY} -O binary bin/crescent crescent.bin
	DEPENDS crescent
)

add_custom_target(run
	COMMAND qemu-system-aarch64 -kernel crescent.bin -initrd initramfs.tar ${QEMU_FLAGS}
	DEPENDS crescent.bin initramfs.tar USES_TERMINAL VERBATIM
)

add_custom_target(debug
	COMMAND qemu-system-aarch64 -kernel crescent.bin -initrd initramfs.tar ${QEMU_FLAGS} -s -S
	DEPENDS crescent.bin initramfs.tar USES_TERMINAL VERBATIM
)

add_custom_command(OUTPUT boot.img
	COMMAND mkbootimg
		--header_version "2"
		--os_version "11.0.0"
		--os_patch_level "2023-11"
		--kernel "crescent.bin"
		--ramdisk "initramfs.tar"
		--dtb "${PROJECT_SOURCE_DIR}/dtb"
		--pagesize "0x1000"
		--base "0x0"
		--kernel_offset "0x8000"
		--ramdisk_offset "0x1000000"
		--second_offset "0x0"
		--tags_offset "0x100"
		--dtb_offset "0x1F00000"
		--board "\"\""
		-o boot.img
	DEPENDS crescent.bin initramfs.tar
)

add_custom_target(generate_boot_img
	DEPENDS boot.img
)
