#pragma once
#include "types.hpp"

#define EI_NIDENT 16

enum class ElfType : u16 {
	None = 0,
	Rel = 1,
	Exec = 2,
	Dyn = 3
};

enum class ElfMachine : u16 {
	X86_64 = 0x3E,
	AArch64 = 0xB7
};

struct Elf64_Ehdr {
	unsigned char e_ident[EI_NIDENT];
	ElfType e_type;
	ElfMachine e_machine;
	u32 e_version;
	u64 e_entry;
	u64 e_phoff;
	u64 e_shoff;
	u32 e_flags;
	u16 e_ehsize;
	u16 e_phentsize;
	u16 e_phnum;
	u16 e_shentsize;
	u16 e_shnum;
	u16 e_shstrndx;
};

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define ELFMAG0 0x7F
#define ELFMAG1 'E'
#define ELFMAG2 'L'
#define ELFMAG3 'F'

#define EI_CLASS 4
#define ELFCLASS32 1
#define ELFCLASS64 2

#define EI_DATA 5
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EI_VERSION 6
#define EV_CURRENT 1

enum class PhdrType : u32 {
	Null = 0,
	Load = 1,
	Dynamic = 2,
	Interp = 3,
	Note = 4,
	Shlib = 5,
	Phdr = 6,
	Tls = 7
};

struct Elf64_Phdr {
	PhdrType p_type;
	u32 p_flags;
	u64 p_offset;
	u64 p_vaddr;
	u64 p_paddr;
	u64 p_filesz;
	u64 p_memsz;
	u64 p_align;
};

#define PF_X 0x1
#define PF_W 0x2
#define PF_R 0x4
