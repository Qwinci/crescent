#pragma once
#include "fs/vfs.hpp"
#include "expected.hpp"

struct LoadedElf {
	void (*entry)(void*);
};

enum class ElfLoadError {
	Invalid,
	NoMemory
};

struct Process;

kstd::expected<LoadedElf, ElfLoadError> elf_load(Process* process, VNode* file);
