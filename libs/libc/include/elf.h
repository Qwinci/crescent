#ifndef _ELF_H
#define _ELF_H

#include <stdint.h>

typedef uint64_t Elf64_Xword;
typedef int64_t Elf64_Sxword;
typedef uint64_t Elf64_Addr;

typedef struct {
	Elf64_Sxword d_tag;
	union {
		Elf64_Xword d_val;
		Elf64_Addr d_ptr;
	} d_un;
} Elf64_Dyn;

#define DT_NULL 0
#define DT_RELA 7
#define DT_RELASZ 8

typedef struct {
	Elf64_Addr r_offset;
	Elf64_Xword r_info;
	Elf64_Xword r_addend;
} Elf64_Rela;

#define ELF64_R_SYM(info) ((info) >> 32)
#define ELF64_R_TYPE(info) ((info) & 0xFFFFFFFF)
#define ELF64_R_INFO(sym, type) (((Elf64_Xword) (sym) << 32) | (type))

#define R_X86_64_RELATIVE 8

#define R_AARCH64_RELATIVE 1027

#endif
