OUTPUT_FORMAT(elf64-littleaarch64)
OUTPUT_ARCH(aarch64)
ENTRY(asm_start)

PHDRS {
	text    PT_LOAD     FLAGS(1 << 0 | 1 << 2);
	rodata  PT_LOAD     FLAGS(1 << 2);
	data    PT_LOAD     FLAGS(1 << 1 | 1 << 2);
	dynamic PT_DYNAMIC  FLAGS(1 << 1 | 1 << 2);
}

SECTIONS {
	. = 0xFFFFFFFF80000000;
    /*. = 0x40080000;*/
	KERNEL_START = .;

	TEXT_START = .;
    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    } :text
    TEXT_END = .;

    . = ALIGN(CONSTANT(MAXPAGESIZE));

	RODATA_START = .;
	.rodata : {
		*(.rodata .rodata.*)
	} :rodata
	.drivers : {
		DRIVERS_START = .;
		KEEP(*(.drivers))
		DRIVERS_END = .;
	}
	RODATA_END = .;

	. = ALIGN(CONSTANT(MAXPAGESIZE));

	DATA_START = .;
    .data : {
        *(.data .data.*)
    } :data

    .dynamic : {
        *(.dynamic)
    } :data :dynamic

	.init_array : {
        INIT_ARRAY_START = .;
        crti.o(.init_array)
        KEEP(*(SORT(EXCLUDE_FILE(crti.o crtn.o) .init_array.*)))
        KEEP(*(EXCLUDE_FILE(crti.o crtn.o) .init_array))
        crtn.o(.init_array)
        INIT_ARRAY_END = .;
    } :data

	.bss : {
		BSS_START = .;
		*(.bss .bss.*)
		*(COMMON)
		BSS_END = .;
	} :data
	DATA_END = .;

	. = ALIGN(CONSTANT(MAXPAGESIZE));
	KERNEL_END = .;

	/DISCARD/ : {
		*(.eh_frame)
		*(.eh_frame_hdr)
		*(.note .note.*)
	}
}
