set(FILES
	${PROJECT_SOURCE_DIR}/kernel/fonts/Tamsyn8x16r.psf
	${PROJECT_SOURCE_DIR}/kernel/limine.cfg
	bin/crescent
	initramfs.tar
)

set(QEMU_FLAGS -m 2G -machine q35 -smp 1
	-no-reboot -no-shutdown -cpu qemu64,+umip,+smep,+smap
	-drive file=nvm.img,if=none,id=nvm,format=raw
	-device nvme,serial=deadbeef,drive=nvm -M smm=off
	-device qemu-xhci
	#-device usb-kbd
	-netdev user,id=mynet0 -device virtio-net-pci-non-transitional,netdev=mynet0
	-device ich9-intel-hda,bus=pcie.0,addr=0x1B#,debug=3
	-device hda-output,audiodev=hda#,debug=3
	-audiodev pa,id=hda
	-serial stdio
	#-device vfio-pci,host=00:1f.3
)

find_program(XORRISO xorriso REQUIRED)
find_program(CP cp REQUIRED)
find_program(GIT git REQUIRED)
find_program(MKDIR mkdir REQUIRED)

add_custom_command(OUTPUT image.iso DEPENDS ${FILES} crescent
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
	find_program(FALLOCATE fallocate REQUIRED)
	find_program(PARTED parted REQUIRED)
	find_program(MKFS_EXT4 mkfs.ext4 REQUIRED)
	execute_process(
		COMMAND ${FALLOCATE} -l 1G ${PROJECT_BINARY_DIR}/nvm.img
		OUTPUT_QUIET ERROR_QUIET)
	execute_process(COMMAND ${PARTED} -s ${PROJECT_BINARY_DIR}/nvm.img mklabel gpt)
	execute_process(COMMAND ${PARTED} -s ${PROJECT_BINARY_DIR}/nvm.img mkpart ESP fat32 2048s 100%)
	# 512 * 2048 == 1048576
	execute_process(COMMAND ${MKFS_EXT4} ${PROJECT_BINARY_DIR}/nvm.img -E offset=1048576 261632k OUTPUT_QUIET ERROR_QUIET)
	execute_process(COMMAND mkdir -p ${PROJECT_BINARY_DIR}/sysroot)
endif()

find_program(GUESTMOUNT guestmount)

add_custom_command(OUTPUT initramfs.tar
	COMMAND tar -cf initramfs.tar -C ${PROJECT_BINARY_DIR}/sysroot/ .
	DEPENDS ${APPS}
	USES_TERMINAL VERBATIM
)

if(GUESTMOUNT)
	add_custom_target(update_image
		COMMAND mkdir -p ${PROJECT_BINARY_DIR}/mountpoint
		COMMAND guestmount --pid-file guestfs.pid -a ${PROJECT_BINARY_DIR}/nvm.img -m /dev/sda1 ${PROJECT_BINARY_DIR}/mountpoint
		COMMAND rsync -a --delete ${PROJECT_BINARY_DIR}/sysroot/ ${PROJECT_BINARY_DIR}/mountpoint/
		COMMAND guestunmount ${PROJECT_BINARY_DIR}/mountpoint
		DEPENDS ${APPS}
		USES_TERMINAL VERBATIM
	)
else()
	add_custom_target(update_image
		COMMAND mkdir -p ${PROJECT_BINARY_DIR}/mountpoint
		COMMAND sudo losetup -Pf --show ${PROJECT_BINARY_DIR}/nvm.img &> loopback.id
		COMMAND sudo mount /dev/loop0p1 ${PROJECT_BINARY_DIR}/mountpoint
		COMMAND sudo rsync -a --delete ${PROJECT_BINARY_DIR}/sysroot/ ${PROJECT_BINARY_DIR}/mountpoint/
		COMMAND sudo umount ${PROJECT_BINARY_DIR}/mountpoint
		COMMAND sudo losetup -d /dev/loop0
		DEPENDS ${APPS}
		USES_TERMINAL VERBATIM
	)
endif()

add_custom_target(run_kvm
	COMMAND qemu-system-x86_64
	-boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
	${QEMU_FLAGS} -enable-kvm -cpu host,+invtsc
	DEPENDS image.iso update_image USES_TERMINAL VERBATIM)

add_custom_target(run
	COMMAND qemu-system-x86_64
	-boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
	${QEMU_FLAGS}
	DEPENDS image.iso update_image USES_TERMINAL VERBATIM)

add_custom_target(debug
	COMMAND qemu-system-x86_64
	-boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
	${QEMU_FLAGS} -S -s
	DEPENDS image.iso update_image USES_TERMINAL VERBATIM)

add_custom_target(debug_kvm
	COMMAND qemu-system-x86_64
	-boot d -cdrom ${PROJECT_BINARY_DIR}/image.iso
	${QEMU_FLAGS} -enable-kvm -S -s
	DEPENDS image.iso update_image USES_TERMINAL VERBATIM)
