#pragma once
#include "types.h"

#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3

#define ELFMAG0 0x7F
#define ELFMAG1 0x45
#define ELFMAG2 0x4C
#define ELFMAG3 0x46

#define EI_CLASS 4

#define ELFCLASSNONE 0
#define ELFCLASS32 1
#define ELFCLASS64 2

#define EI_DATA 5

#define ELFDATANONE 0
#define ELFDATA2LSB 1
#define ELFDATA2MSB 2

#define EI_VERSION 6

#define EV_NONE 0
#define EV_CURRENT 1

#define ET_NONE 0
#define ET_REL 1
#define ET_EXEC 2
#define ET_DYN 3
#define ET_CORE 4

#define EM_X86_64 0x3E

typedef struct {
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
} Elf64EHdr;

#define PT_LOAD 0x1

typedef struct {
	u32 p_type;
	u32 p_flags;
	u64 p_offset;
	u64 p_vaddr;
	u64 p_paddr;
	u64 p_filesz;
	u64 p_memsz;
	u64 p_align;
} Elf64PHdr;

#define ELF_PF_X 1
#define ELF_PF_W 2
#define ELF_PF_R 4

typedef enum : u32 {
	ELF_SH_NULL = 0,
	ELF_SH_PROGBITS = 1,
	ELF_SH_SYMTAB = 2,
	ELF_SH_STRTAB = 3,
	ELF_SH_RELA = 4,
	ELF_SH_HASH = 5,
	ELF_SH_DYNAMIC = 6,
	ELF_SH_NOTE = 7,
	ELF_SH_NOBITS = 8,
	ELF_SH_REL = 9,
	ELF_SH_SHLIB = 0xA,
	ELF_SH_DYNSYM = 0xB,
	ELF_SH_INITARRAY = 0xE,
	ELF_SH_FINIARRAY = 0xF,
	ELF_SH_PREINITARRAY = 0x10,
	ELF_SH_GROUP = 0x11,
	ELF_SH_SYMTABSHNDX = 0x12,
	ELF_SH_NUM = 0x13
} ElfShType;

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

typedef struct {
	u32 sh_name;
	ElfShType sh_type;
	u64 sh_flags;
	u64 sh_addr;
	u64 sh_offset;
	u64 sh_size;
	u32 sh_link;
	u32 sh_info;
	u64 sh_addralign;
	u64 sh_entsize;
} Elf64SHdr;

typedef struct {
	u32 st_name;
	u8 st_info;
	u8 st_other;
	u16 st_shndx;
	u64 st_value;
	u64 st_size;
} Elf64Sym;

#define ELF64_ST_BIND(info) ((info) >> 4)
#define ELF64_ST_TYPE(info) ((info) & 0xF)

#define STB_LOCAL 0
#define STB_GLOBAL 1
#define STB_WEAK 2
#define STB_LOOS 10
#define STB_HIOS 12
#define STB_LOPROC 13
#define STB_HIPROC 15

#define STT_NOTYPE 0
#define STT_OBJECT 1
#define STT_FUNC 2
#define STT_SECTION 3
#define STT_FILE 4
#define STT_COMMON 5
#define STT_TLS 6
#define STT_LOOS 10
#define STT_HIOS 12
#define STT_LOPROC 13
#define STT_HIPROC 15

#define ELF64_ST_VISIBILITY(other) ((other) & 0x3)

#define STV_DEFAULT 0
#define STV_INTERNAL 1
#define STV_HIDDEN 2
#define STV_PROTECTED 3
#define STV_EXPORTED 4
#define STV_SINGLETON 5
#define STV_ELIMINATE 6

typedef struct {
	u64 r_offset;
	u64 r_info;
} Elf64Rel;

typedef struct {
	u64 r_offset;
	u64 r_info;
	i64 r_addend;
} Elf64Rela;

#define ELF64_R_SYM(info) ((info) >> 32)
#define ELF64_R_TYPE(info) ((u32) (info))
#define ELF64_R_INFO(sym, type) (((u64) (sym) << 32) + (u64) (type))

#define R_AMD64_NONE 0
#define R_AMD64_64 1
#define R_AMD64_PC32 2
#define R_AMD64_GOT32 3
#define R_AMD64_PLT32 4
#define R_AMD64_COPY 5
#define R_AMD64_GLOB_DAT 6
#define R_AMD64_JUMP_SLOT 7
#define R_AMD64_RELATIVE 8
#define R_AMD64_GOTPCREL 9
#define R_AMD64_32 10
#define R_AMD64_32S 11
#define R_AMD64_16 12
#define R_AMD64_PC16 13
#define R_AMD64_8 14
#define R_AMD64_PC8 15
#define R_AMD64_PC64 24
#define R_AMD64_GOTOFF64 25
#define R_AMD64_GOTPC32 26
#define R_AMD64_SIZE32 32
#define R_AMD64_SIZE64 33