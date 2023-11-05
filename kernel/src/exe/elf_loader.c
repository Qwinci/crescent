#include "elf_loader.h"
#include "arch/map.h"
#include "assert.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "string.h"
#include "utils/elf.h"
#include "crescent/sys.h"
#include "fs/vfs.h"
#include "mem/allocator.h"
#include "mem/vm.h"
#include "sched/task.h"

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
	usize largest_addr = 0;
	for (u16 i = 0; i < e_hdr->e_phnum; ++i) {
		const Elf64PHdr* p_hdr = offset(e_hdr, const Elf64PHdr*, e_hdr->e_phoff + i * e_hdr->e_phentsize);
		if (p_hdr->p_type == PT_LOAD) {
			if (!base_found) {
				res.base = p_hdr->p_vaddr;
				base_found = true;
			}
			res.mem_size += ALIGNUP(p_hdr->p_memsz, p_hdr->p_align);
			if (p_hdr->p_vaddr - res.base + p_hdr->p_memsz > largest_addr) {
				largest_addr = p_hdr->p_vaddr - res.base + p_hdr->p_memsz;
			}
		}
	}

	res.file_base = data;
	res.mem_size = largest_addr;
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

int elf_load_from_file(Process* process, VNode* node, LoadedElf* res, bool relocate, usize* interp_base) {
	Elf64EHdr ehdr;
	int ret = 0;
	if ((ret = node->ops.read(node, &ehdr, 0, sizeof(Elf64EHdr))) != 0) {
		return ERR_INVALID_ARG;
	}

	CrescentStat s;
	if ((ret = node->ops.stat(node, &s)) != 0) {
		return ret;
	}

	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
	    ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
	    ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
	    ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
	    ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
	    ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
	    ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
	    ehdr.e_machine != EM_X86_64 ||
	    (ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
		ehdr.e_phentsize != sizeof(Elf64PHdr) ||
		ehdr.e_shentsize != sizeof(Elf64SHdr) ||
		ehdr.e_phoff >= s.size ||
		ehdr.e_shoff >= s.size ||
		ehdr.e_phnum * sizeof(Elf64PHdr) >= s.size) {
		return ERR_INVALID_ARG;
	}

	bool relocatable = ehdr.e_type == ET_DYN;

	Elf64PHdr* phdrs = (Elf64PHdr*) kmalloc(ehdr.e_phnum * ehdr.e_phentsize);
	if (!phdrs) {
		return ERR_NO_MEM;
	}

	typedef struct {
		void* mapping;
		usize size;
	} ElfMapping;

	ElfMapping* mappings = (ElfMapping*) kcalloc(ehdr.e_phnum * sizeof(ElfMapping));
	if (!mappings) {
		kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
		return ERR_NO_MEM;
	}
	if ((ret = node->ops.read(node, phdrs, ehdr.e_phoff, ehdr.e_phnum * ehdr.e_phentsize)) != 0) {
		kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
		return ERR_INVALID_ARG;
	}

	usize base = relocatable ? 0x400000 : 0;
	if (interp_base) {
		base = 0x10000000;
		*interp_base = base;
	}

	for (u16 i = 0; i < ehdr.e_phnum; ++i) {
		const Elf64PHdr* phdr = offset(phdrs, Elf64PHdr*, i * ehdr.e_phentsize);
		if (phdr->p_type == PT_LOAD) {
			if (!phdr->p_memsz) {
				continue;
			}

			bool is_aligned = phdr->p_offset % phdr->p_align == phdr->p_vaddr % phdr->p_align;

			usize align = phdr->p_vaddr & (PAGE_SIZE - 1);
			usize aligned_addr = base + phdr->p_vaddr - align;
			usize size = ALIGNUP(phdr->p_memsz + align, PAGE_SIZE);

			if (!is_aligned) {
				kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
				return ERR_INVALID_ARG;
			}

			PageFlags flags = PF_USER;
			if (phdr->p_flags & ELF_PF_R) {
				flags |= PF_READ;
			}
			if (phdr->p_flags & ELF_PF_W) {
				flags |= PF_WRITE;
			}
			if (phdr->p_flags & ELF_PF_X) {
				flags |= PF_EXEC;
			}

			void* mapping;
			bool high = aligned_addr >= (usize) process->high_vmem.base;
			void* mem = vm_user_alloc_backed(process, (void*) aligned_addr, size / PAGE_SIZE, flags, high, &mapping);
			if (!mem) {
				for (u16 j = 0; j < i; ++j) {
					const Elf64PHdr* phdr2 = offset(phdrs, Elf64PHdr*, j * ehdr.e_phentsize);
					if (phdr2->p_type == PT_LOAD) {
						if (!phdr2->p_memsz) {
							continue;
						}
						usize align2 = phdr2->p_vaddr & (PAGE_SIZE - 1);
						usize aligned_addr2 = base + phdr2->p_vaddr - align2;
						usize size2 = ALIGNUP(phdr2->p_memsz + align2, PAGE_SIZE);
						vm_user_dealloc_backed(process, (void*) aligned_addr2, size2 / PAGE_SIZE, high, NULL);
					}
				}

				kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
				return ERR_NO_MEM;
			}

			memset(mapping, 0, size);
			if ((ret = node->ops.read(node, offset(mapping, void*, align), phdr->p_offset, phdr->p_filesz)) != 0) {
				for (u16 j = 0; j < i; ++j) {
					const Elf64PHdr* phdr2 = offset(phdrs, Elf64PHdr*, j * ehdr.e_phentsize);
					if (phdr2->p_type == PT_LOAD) {
						if (!phdr2->p_memsz) {
							continue;
						}
						usize align2 = phdr2->p_vaddr & (PAGE_SIZE - 1);
						usize aligned_addr2 = base + phdr2->p_vaddr - align2;
						usize size2 = ALIGNUP(phdr2->p_memsz + align2, PAGE_SIZE);
						vm_user_dealloc_backed(process, (void*) aligned_addr2, size2 / PAGE_SIZE, high, NULL);
					}
				}

				kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
				return ERR_INVALID_ARG;
			}

			mappings[i].mapping = mapping;
			mappings[i].size = size;
		}
	}

	Elf64Dyn* dyn_ptr = NULL;
	usize dyn_size = 0;
	if (relocatable) {
		for (u16 i = 0; i < ehdr.e_phnum; ++i) {
			Elf64PHdr* phdr = offset(phdrs, Elf64PHdr*, i * ehdr.e_phentsize);
			if (phdr->p_type == PT_DYNAMIC) {
				dyn_ptr = kmalloc(phdr->p_filesz);
				if (!dyn_ptr) {
					for (usize j = 0; j < ehdr.e_phnum; ++j) {
						ElfMapping m = mappings[j];
						if (!m.mapping) {
							continue;
						}
						vm_user_dealloc_kernel(m.mapping, m.size / PAGE_SIZE);
					}
					return ERR_NO_MEM;
				}

				if (node->ops.read(node, dyn_ptr, phdr->p_offset, phdr->p_filesz) != 0) {
					for (usize j = 0; j < ehdr.e_phnum; ++j) {
						ElfMapping m = mappings[j];
						if (!m.mapping) {
							continue;
						}
						vm_user_dealloc_kernel(m.mapping, m.size / PAGE_SIZE);
					}
					kfree(dyn_ptr, phdr->p_filesz);
					return ERR_INVALID_ARG;
				}
				dyn_size = phdr->p_filesz;
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
			if (relocate && rela_size) {
				Elf64Rela* rela = kmalloc(rela_size);
				if (!rela) {
					for (usize j = 0; j < ehdr.e_phnum; ++j) {
						ElfMapping m = mappings[j];
						if (!m.mapping) {
							continue;
						}
						vm_user_dealloc_kernel(m.mapping, m.size / PAGE_SIZE);
					}
					kfree(dyn_ptr, dyn_size);
					kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
					kfree(mappings, ehdr.e_phnum * sizeof(ElfMapping));
					return ERR_NO_MEM;
				}
				if (node->ops.read(node, rela, rela_off, rela_size) != 0) {
					for (usize j = 0; j < ehdr.e_phnum; ++j) {
						ElfMapping m = mappings[j];
						if (!m.mapping) {
							continue;
						}
						vm_user_dealloc_kernel(m.mapping, m.size / PAGE_SIZE);
					}
					kfree(dyn_ptr, dyn_size);
					kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
					kfree(mappings, ehdr.e_phnum * sizeof(ElfMapping));
					return ERR_INVALID_ARG;
				}

				for (usize i = 0; i < rela_size / sizeof(Elf64Rela); ++i, ++rela) {
					u64 type = ELF64_R_TYPE(rela->r_info);

					usize addr = rela->r_offset;

					ElfMapping* m = NULL;
					usize phdr_vaddr = 0;
					for (usize j = 0; j < ehdr.e_phnum; ++j) {
						const Elf64PHdr* phdr = offset(phdrs, const Elf64PHdr*, j * ehdr.e_phentsize);

						if (addr >= phdr->p_vaddr && addr < phdr->p_vaddr + phdr->p_memsz) {
							m = &mappings[j];
							phdr_vaddr = phdr->p_vaddr;
							break;
						}
					}

					if (!m) {
						for (usize j = 0; j < ehdr.e_phnum; ++j) {
							ElfMapping m2 = mappings[j];
							if (!m2.mapping) {
								continue;
							}
							vm_user_dealloc_kernel(m2.mapping, m2.size / PAGE_SIZE);
						}
						kfree(dyn_ptr, dyn_size);
						kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
						kfree(mappings, ehdr.e_phnum * sizeof(ElfMapping));
						return ERR_INVALID_ARG;
					}

					usize align = phdr_vaddr & (PAGE_SIZE - 1);
					if (type == R_AMD64_RELATIVE) {
						*offset(m->mapping, u64*, align + addr - phdr_vaddr) = base + rela->r_addend;
					}
					else {
						panic("unsupported elf relocation type %u\n", type);
					}
				}

				kfree(rela, rela_size);
			}
		}
	}

	for (usize j = 0; j < ehdr.e_phnum; ++j) {
		ElfMapping m2 = mappings[j];
		if (!m2.mapping) {
			continue;
		}
		vm_user_dealloc_kernel(m2.mapping, m2.size / PAGE_SIZE);
	}

	kfree(phdrs, ehdr.e_phnum * ehdr.e_phentsize);
	kfree(mappings, ehdr.e_phnum * sizeof(ElfMapping));
	kfree(dyn_ptr, dyn_size);

	res->entry = relocatable ? (void*) (base + ehdr.e_entry) : (void*) ehdr.e_entry;

	return 0;
}

int elf_get_interp(VNode* node, char** interp, usize* interp_len) {
	Elf64EHdr ehdr;
	if (node->ops.read(node, &ehdr, 0, sizeof(Elf64EHdr)) != 0) {
		return ERR_INVALID_ARG;
	}

	CrescentStat s;
	if (node->ops.stat(node, &s) != 0) {
		return ERR_INVALID_ARG;
	}

	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_machine != EM_X86_64 ||
		(ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
		ehdr.e_phentsize != sizeof(Elf64PHdr) ||
		ehdr.e_shentsize != sizeof(Elf64SHdr) ||
		ehdr.e_phoff >= s.size ||
		ehdr.e_shoff >= s.size ||
		ehdr.e_phnum * sizeof(Elf64PHdr) >= s.size) {
		return ERR_INVALID_ARG;
	}

	for (u16 i = 0; i < ehdr.e_phnum; ++i) {
		Elf64PHdr phdr;
		if (node->ops.read(node, &phdr, ehdr.e_phoff + i * ehdr.e_phentsize, sizeof(Elf64PHdr)) != 0) {
			return ERR_INVALID_ARG;
		}
		if (phdr.p_type == PT_INTERP) {
			char* mem = (char*) kmalloc(phdr.p_filesz);
			if (!mem) {
				return ERR_NO_MEM;
			}
			if (node->ops.read(node, mem, phdr.p_offset, phdr.p_filesz) != 0) {
				kfree(mem, phdr.p_filesz);
				return ERR_INVALID_ARG;
			}
			*interp = mem;
			*interp_len = phdr.p_filesz;
			return 0;
		}
	}

	return ERR_NOT_EXISTS;
}

int elf_load_user_phdrs(Process* process, VNode* node, void** user_mem, usize* user_phdr_count, usize* user_entry) {
	Elf64EHdr ehdr;
	int ret = 0;
	if ((ret = node->ops.read(node, &ehdr, 0, sizeof(Elf64EHdr))) != 0) {
		return ERR_INVALID_ARG;
	}

	CrescentStat s;
	if ((ret = node->ops.stat(node, &s)) != 0) {
		return ret;
	}

	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
		ehdr.e_ident[EI_VERSION] != EV_CURRENT ||
		ehdr.e_machine != EM_X86_64 ||
		(ehdr.e_type != ET_EXEC && ehdr.e_type != ET_DYN) ||
		ehdr.e_phentsize != sizeof(Elf64PHdr) ||
		ehdr.e_shentsize != sizeof(Elf64SHdr) ||
		ehdr.e_phoff >= s.size ||
		ehdr.e_shoff >= s.size ||
		ehdr.e_phnum * sizeof(Elf64PHdr) >= s.size) {
		return ERR_INVALID_ARG;
	}

	void* mapping;
	void* phdrs = vm_user_alloc_backed(process, NULL, ALIGNUP(ehdr.e_phnum * ehdr.e_phentsize, PAGE_SIZE) / PAGE_SIZE, PF_USER | PF_READ | PF_WRITE, true, &mapping);
	if (!phdrs) {
		return ERR_NO_MEM;
	}
	if ((ret = node->ops.read(node, mapping, ehdr.e_phoff, ehdr.e_phnum * ehdr.e_phentsize)) != 0) {
		vm_user_dealloc_backed(process, phdrs, ALIGNUP(ehdr.e_phnum * ehdr.e_phentsize, PAGE_SIZE) / PAGE_SIZE, true, mapping);
		return ERR_INVALID_ARG;
	}
	*user_mem = phdrs;
	*user_phdr_count = ehdr.e_phnum;
	*user_entry = ehdr.e_entry;
	return 0;
}
