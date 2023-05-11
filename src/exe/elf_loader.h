#pragma once
#include "types.h"

typedef struct {
	void* entry;
} LoadedElf;

typedef struct {
	const void* file_base;
	usize base;
	usize mem_size;
	bool relocatable;
	bool good;
} ElfInfo;

/// Gets elf file info from memory
ElfInfo elf_get_info(const void* data);

/// Loads an elf file from memory
/// @param info Elf info from elf_get_info
/// @param load_base Memory base to use for memory accesses
/// @param run_base Memory base to use for relocation calculations
LoadedElf elf_load(ElfInfo info, void* load_base, void* run_base);

void elf_protect(ElfInfo info, void* load_base, void* map, bool user);
