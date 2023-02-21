ENTRY(start)

SECTIONS {
	. = 0xFFFFFFFF80000000 + SIZEOF_HEADERS;
	PROVIDE(RODATA_START = 0xFFFFFFFF80000000);
	.hash : { *(.hash) }
	.gnu.hash : { *(.gnu.hash) }
	.dynsym : { *(.dynsym) }
	.dynstr : { *(.dynstr) }
	.rela : { *(.rela*) }
	.rodata : { *(.rodata .rodata.*) }
	.eh_frame_hdr : { *(.eh_frame_hdr) }
	.eh_frame : { KEEP(*(.eh_frame)) }
	.drivers : {
	    DRIVERS_START = .;
	    KEEP(*(.drivers))
	    DRIVERS_END = .;
	}
	. = ALIGN(CONSTANT(MAXPAGESIZE));
	PROVIDE(RODATA_END = .);

	PROVIDE(TEXT_START = .);
	.plt : { *(.plt) *(.iplt) }
	.text : { *(.text .text.*) }
	. = ALIGN(CONSTANT(MAXPAGESIZE));
	PROVIDE(TEXT_END = .);

	PROVIDE(DATA_START = .);
	.tdata : { *(.tdata .tdata.*) }
	.tbss : { *(.tbss .tbss.*) }

	.data.rel.ro : {
		*(.data.rel.ro .data.rel.ro.*)
	}

	.dynamic : { *(.dynamic) }

	.init_array : {
		__init_array_start = .;
		crti.o(.init_array)
		KEEP(*(SORT(EXCLUDE_FILE(crti.o crtn.o) .init_array.*)))
		KEEP(*(EXCLUDE_FILE(crti.o crtn.o) .init_array))
		crtn.o(.init_array)
		__init_array_end = .;
	}

	.fini_array : {
		__fini_array_start = .;
		crti.o(.fini_array)
		KEEP(*(SORT(EXCLUDE_FILE(crti.o crtn.o) .fini_array.*)))
		KEEP(*(EXCLUDE_FILE(crti.o crtn.o) .fini_array))
		crtn.o(.fini_array)
		__fini_array_end = .;
	}

	. = DATA_SEGMENT_RELRO_END(0, .);

	.got : { *(.got) *(.igot) }
	.got.plt : { *(.got.plt) *(.igot.plt) }
	.data : { *(.data .data.*) }
	.bss : { *(COMMON) *(.dynbss) *(.bss .bss.*) }

	. = DATA_SEGMENT_END(.);
	PROVIDE(DATA_END = .);
	PROVIDE(KERNEL_END = .);

	.comment 0 : { *(.comment) }
	.debug 0 : { *(.debug) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_aranges 0 : { *(.debug_aranges) }
	.debug_frame 0 : { *(.debug_frame) }
	.debug_funcnames 0 : { *(.debug_funcnames) }
	.debug_info 0 : { *(.debug_info) }
	.debug_line 0 : { *(.debug_line) }
	.debug_loc 0 : { *(.debug_loc) }
	.debug_macinfo 0 : { *(.debug_macinfo) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_pubtypes 0 : { *(.debug_pubtypes) }
	.debug_ranges 0 : { *(.debug_ranges) }
	.debug_sfnames 0 : { *(.debug_sfnames) }
	.debug_srcinfo 0 : { *(.debug_srcinfo) }
	.debug_str 0 : { *(.debug_str) }
	.debug_typenames 0 : { *(.debug_typenames) }
	.debug_varnames 0 : { *(.debug_varnames) }
	.debug_weaknames 0 : { *(.debug_weaknames) }
	.line 0 : { *(.line) }
	.shstrtab 0 : { *(.shstrtab) }
	.strtab 0 : { *(.strtab) }
	.symtab 0 : { *(.symtab) }

	/DISCARD/ : {
		*(.note.*)
	}
}