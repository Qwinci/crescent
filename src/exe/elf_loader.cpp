#include "elf_loader.hpp"
#include "elf.hpp"
#include "sched/process.hpp"

#ifdef __x86_64__
static constexpr ElfMachine CURRENT_MACHINE = ElfMachine::X86_64;
#elif defined(__aarch64__)
static constexpr ElfMachine CURRENT_MACHINE = ElfMachine::AArch64;
#else
#error Unsupported architecture
#endif

kstd::expected<LoadedElf, ElfLoadError> elf_load(Process* process, VNode* file) {
	FsStat stat {};
	if (file->stat(stat) != FsStatus::Success) {
		return ElfLoadError::Invalid;
	}

	Elf64_Ehdr ehdr {};
	usize to_read = sizeof(Elf64_Ehdr);
	if (auto status = file->read(&ehdr, to_read, 0); status != FsStatus::Success) {
		return ElfLoadError::Invalid;
	}
	if (ehdr.e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr.e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr.e_ident[EI_MAG3] != ELFMAG3 ||
		ehdr.e_ident[EI_CLASS] != ELFCLASS64 ||
		ehdr.e_ident[EI_DATA] != ELFDATA2LSB ||
		ehdr.e_machine != CURRENT_MACHINE ||
		(ehdr.e_type != ElfType::Exec && ehdr.e_type != ElfType::Dyn) ||
		ehdr.e_phnum >= 512 ||
		ehdr.e_phentsize >= 512 ||
		ehdr.e_phoff >= UINT32_MAX ||
		ehdr.e_phoff + ehdr.e_phnum * ehdr.e_phentsize > stat.size) {
		return ElfLoadError::Invalid;
	}

	kstd::vector<Elf64_Phdr> phdrs;
	phdrs.resize(ehdr.e_phnum);
	to_read = sizeof(Elf64_Phdr);
	for (usize i = 0; i < ehdr.e_phnum; ++i) {
		if (file->read(&phdrs[i], to_read, ehdr.e_phoff + i * ehdr.e_phentsize) != FsStatus::Success) {
			return ElfLoadError::Invalid;
		}
	}

	usize base = 0;
	usize end = 0;
	bool base_found = false;
	for (const auto& phdr : phdrs) {
		if (phdr.p_type == PhdrType::Load) {
			if (!base_found) {
				base = phdr.p_vaddr;
				base_found = true;
			}
			end = kstd::max(end, phdr.p_vaddr + phdr.p_memsz);
		}
	}

	usize map_size = end - base;

	UniqueKernelMapping mapping;
	usize user_mem = process->allocate(
		reinterpret_cast<void*>(base),
		map_size,
		MemoryAllocFlags::Read | MemoryAllocFlags::Write | MemoryAllocFlags::Backed,
		&mapping);
	if (!user_mem) {
		return ElfLoadError::NoMemory;
	}

	usize phdrs_offset = 0;

	for (const auto& phdr : phdrs) {
		if (phdr.p_type == PhdrType::Phdr) {
			phdrs_offset = phdr.p_vaddr - base;
			continue;
		}
		else if (phdr.p_type != PhdrType::Load) {
			continue;
		}

		if (phdr.p_vaddr % PAGE_SIZE != phdr.p_offset % PAGE_SIZE) {
			process->free(user_mem, map_size);
			return ElfLoadError::Invalid;
		}
		void* ptr = offset(mapping.data(), void*, phdr.p_vaddr - base);

		to_read = phdr.p_filesz;
		if (file->read(ptr, to_read, phdr.p_offset) != FsStatus::Success) {
			process->free(user_mem, map_size);
			return ElfLoadError::Invalid;
		}

		if (phdr.p_memsz > phdr.p_filesz) {
			memset(offset(ptr, void*, phdr.p_filesz), 0, phdr.p_memsz - phdr.p_filesz);
		}

		PageFlags flags = PageFlags::User;
		if (phdr.p_flags & PF_R) {
			flags |= PageFlags::Read;
		}
		if (phdr.p_flags & PF_W) {
			flags |= PageFlags::Read | PageFlags::Write;
		}
		if (phdr.p_flags & PF_X) {
			flags |= PageFlags::Read | PageFlags::Execute;
		}

		usize aligned_addr = phdr.p_vaddr & ~(PAGE_SIZE - 1);
		usize align = phdr.p_vaddr & (PAGE_SIZE - 1);
		process->page_map.protect_range(user_mem + (aligned_addr - base), phdr.p_memsz + align, flags, CacheMode::WriteBack);
	}

	return LoadedElf {
		.entry = reinterpret_cast<void (*)(void*)>(user_mem + (ehdr.e_entry - base)),
		.base = user_mem,
		.phdrs_addr = user_mem + phdrs_offset,
		.phdr_count = ehdr.e_phnum,
		.phdr_size = ehdr.e_phentsize
	};
}
