OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

PHDRS {
	text	PT_LOAD	 FLAGS(1 << 0 | 1 << 2);
	rodata  PT_LOAD	 FLAGS(1 << 2);
	data	PT_LOAD	 FLAGS(1 << 1 | 1 << 2);
	cpu_local PT_NOTE FLAGS(1 << 2);
	dynamic PT_DYNAMIC  FLAGS(1 << 1 | 1 << 2);
}

SECTIONS {
	KERNEL_START = 0xFFFFFFFF80000000;
	. = 0xFFFFFFFF80000000;

	TEXT_START = .;
	.text : {
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
		KEEP(*(SORT(EXCLUDE_FILE(crti.o crtn.o) .init_array.*)))
		KEEP(*(EXCLUDE_FILE(crti.o crtn.o) .init_array))
		INIT_ARRAY_END = .;
	} :data

	.cpu_local_ctors : {
		CPU_LOCAL_CTORS_START = .;
        KEEP(*(.cpu_local_ctors))
        CPU_LOCAL_CTORS_END = .;
    } :data

	.bss : {
		__CPU_LOCAL_START = .;
		*(.bss.cpu_local)
		__CPU_LOCAL_END = .;
		*(.bss .bss.*)
		*(COMMON)
	} :data
	DATA_END = .;

	/DISCARD/ : {
		*(.eh_frame)
		*(.eh_frame_hdr)
		*(.note .note.*)
	}
}
