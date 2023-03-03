#pragma once
#include "types.hpp"

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3

#define ELFMAG0 0x7F
#define ELFMAG1 0x45
#define ELFMAG2 0x4C
#define ELFMAG3 0x46

struct Elf64EHdr {
	u8 e_ident[16];
	u16 e_type;
	u16 e_machine;
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

#define PT_LOAD 0x1

struct Elf64PHdr {
	u32 p_type;
	u32 p_flags;
	u64 p_offset;
	u64 p_vaddr;
	u64 p_paddr;
	u64 p_filesz;
	u64 p_memsz;
	u64 p_align;
};

enum class SectionType : u32 {
	Null = 0,
	ProgBits = 1,
	SymTab = 2,
	StrTab = 3,
	Rela = 4,
	Hash = 5,
	Dynamic = 6,
	Note = 7,
	NoBits = 8,
	Rel = 9,
	ShLib = 0xA,
	DynSym = 0xB,
	InitArray = 0xE,
	FiniArray = 0xF,
	PreInitArray = 0x10,
	Group = 0x11,
	SymTabShndx = 0x12,
	Num = 0x13
};

#define SHF_WRITE 1
#define SHF_ALLOC 2
#define SHF_EXECINSTR 4
#define SHF_MERGE 0x10
#define SHF_STRINGS 0x20
#define SHF_INFO_LINK 0x40
#define SHF_LINK_ORDER 0x80
#define SHF_OS_NONCONFORMING 0x100
#define SHF_GROUP 0x200
#define SHF_TLS 0x400

struct Elf64SHdr {
	u32 sh_name;
	SectionType sh_type;
	u64 sh_flags;
	u64 sh_addr;
	u64 sh_offset;
	u64 sh_size;
	u32 sh_link;
	u32 sh_info;
	u64 sh_addralign;
	u64 sh_entsize;
};