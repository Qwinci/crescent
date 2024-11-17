#pragma once
#include "fs/vfs.hpp"
#include "expected.hpp"

struct LoadedElf {
	void (*entry)(void*);
	usize base;
	usize phdrs_addr;
	u16 phdr_count;
	u16 phdr_size;
};

enum class ElfLoadError {
	Invalid,
	NoMemory
};

struct Process;

kstd::expected<LoadedElf, ElfLoadError> elf_load(Process* process, VNode* file);
