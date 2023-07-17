#include "elf_loader.h"
#include "arch/map.h"
#include "assert.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "string.h"
#include "utils/elf.h"

typedef struct {
	void* base;
	usize size;
} ElfSection;

ElfSection elf_get_section(ElfInfo info, const char* name) {
	const Elf64EHdr* e_hdr = (const Elf64EHdr*) info.file_base;

	const Elf64SHdr* shstrtab_sect = offset(e_hdr, const Elf64SHdr*, e_hdr->e_shoff + e_hdr->e_shstrndx * e_hdr->e_shentsize);
	const char* shstrtab = (const char*) e_hdr + shstrtab_sect->sh_offset;

	for (usize i = 0; i < e_hdr->e_shnum; ++i) {
		const Elf64SHdr* s_hdr = offset(e_hdr, const Elf64SHdr*, e_hdr->e_shoff + i * e_hdr->e_shentsize);
		const char* s_name = shstrtab + s_hdr->sh_name;
		if (strcmp(s_name, name) == 0) {
			return (ElfSection) {.base = offset(e_hdr, void*, s_hdr->sh_offset), .size = s_hdr->sh_size};
		}
	}

	return (ElfSection) {};
}

ElfInfo elf_get_info(const void* data) {
	ElfInfo res = {};

	const Elf64EHdr* e_hdr = (const Elf64EHdr*) data;
	if (!e_hdr ||
		e_hdr->e_ident[EI_MAG0] != ELFMAG0 ||
		e_hdr->e_ident[EI_MAG1] != ELFMAG1 ||
		e_hdr->e_ident[EI_MAG2] != ELFMAG2 ||
		e_hdr->e_ident[EI_MAG3] != ELFMAG3 ||
		e_hdr->e_ident[EI_CLASS] != ELFCLASS64 ||
		e_hdr->e_ident[EI_DATA] != ELFDATA2LSB ||
		e_hdr->e_ident[EI_VERSION] != EV_CURRENT ||
		e_hdr->e_machine != EM_X86_64 ||
		(e_hdr->e_type != ET_EXEC && e_hdr->e_type != ET_DYN)) {
		return res;
	}

	res.relocatable = e_hdr->e_type == ET_DYN;

	bool base_found = false;
	for (u16 i = 0; i < e_hdr->e_phnum; ++i) {
		const Elf64PHdr* p_hdr = offset(e_hdr, const Elf64PHdr*, e_hdr->e_phoff + i * e_hdr->e_phentsize);
		if (p_hdr->p_type == PT_LOAD) {
			if (!base_found) {
				res.base = p_hdr->p_vaddr;
				base_found = true;
			}
			res.mem_size += ALIGNUP(p_hdr->p_memsz, p_hdr->p_align);
		}
	}

	res.file_base = data;
	return res;
}

#define DT_COUNT 34

LoadedElf elf_load(ElfInfo info, void* load_base, void* run_base) {
	assert(load_base);
	memset(load_base, 0, info.mem_size);
	if (!info.relocatable) {
		assert(info.base == (usize) run_base && "attempted to load static executable with different base");
	}

	const Elf64EHdr* e_hdr = (const Elf64EHdr*) info.file_base;

	for (u16 i = 0; i < e_hdr->e_phnum; ++i) {
		const Elf64PHdr* p_hdr = offset(e_hdr, const Elf64PHdr*, e_hdr->e_phoff + i * e_hdr->e_phentsize);
		if (p_hdr->p_type == PT_LOAD) {
			const void* src = offset(e_hdr, const void*, p_hdr->p_offset);
			void* dest = offset(load_base, void*, p_hdr->p_vaddr - info.base);
			memcpy(dest, src, p_hdr->p_filesz);
		}
	}

	LoadedElf res = {};
	res.entry = offset(run_base, void*, e_hdr->e_entry - info.base);

	const Elf64Dyn* dyn_ptr = NULL;
	if (run_base != (void*) info.base) {
		for (u16 i = 0; i < e_hdr->e_phnum; ++i) {
			const Elf64PHdr* p_hdr = offset(e_hdr, const Elf64PHdr*, e_hdr->e_phoff + i * e_hdr->e_phentsize);
			if (p_hdr->p_type == PT_DYNAMIC) {
				dyn_ptr = offset(e_hdr, const Elf64Dyn*, p_hdr->p_offset);
				break;
			}
		}

		if (dyn_ptr) {
			usize dyn[DT_COUNT] = {};
			for (usize i = 0; dyn_ptr[i].d_tag != DT_NULL; ++i) {
				if (dyn_ptr[i].d_tag < DT_COUNT) {
					dyn[dyn_ptr[i].d_tag] = dyn_ptr[i].d_val;
				}
			}

			usize rela_size = dyn[DT_RELASZ];
			usize rela_off = dyn[DT_RELA];
			if (rela_size) {
				const Elf64Rela* rela = offset(e_hdr, const Elf64Rela*, rela_off);

				for (usize i = 0; i < rela_size / sizeof(Elf64Rela); ++i, ++rela) {
					u64 type = ELF64_R_TYPE(rela->r_info);

					usize off = rela->r_offset - info.base;

					if (type == R_AMD64_RELATIVE) {
						*offset(load_base, u64*, off) = (usize) run_base + rela->r_addend;
					}
					else {
						panic("unsupported elf relocation type %u\n", type);
					}
				}
			}
		}
	}

	return res;
}

void elf_protect(ElfInfo info, void* load_base, void* map, bool user) {
	const Elf64EHdr* e_hdr = (const Elf64EHdr*) info.file_base;

	for (u16 i = 0; i < e_hdr->e_phnum; ++i) {
		const Elf64PHdr* p_hdr = offset(e_hdr, const Elf64PHdr*, e_hdr->e_phoff + i * e_hdr->e_phentsize);
		if (p_hdr->p_type == PT_LOAD) {
			void* dest = offset(load_base, void*, p_hdr->p_vaddr - info.base);
			PageFlags flags = user ? PF_USER : 0;
			flags |= p_hdr->p_flags & ELF_PF_X ? PF_EXEC : 0;
			flags |= p_hdr->p_flags & ELF_PF_W ? PF_WRITE : 0;
			flags |= p_hdr->p_flags & ELF_PF_R ? PF_READ : 0;
			for (usize j = 0; j < ALIGNUP(p_hdr->p_memsz, p_hdr->p_align); j += PAGE_SIZE) {
				arch_protect_page(map, offset(dest, usize, j), flags);
			}
		}
	}
}
