#include "arch/misc.h"
#include "arch/x86/mod.h"
#include "assert.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "mem/vm.h"
#include "sched/sched.h"
#include "stdio.h"
#include "string.h"
#include "utils/elf.h"

void task() {
	while (true) {
		kprintf("a\n");
		sched_sleep(US_IN_MS * 1000);
	}
}

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");

	Module user_file = x86_module_get("basic");

	const Elf64EHdr* ehdr = (const Elf64EHdr*) user_file.base;
	if (!ehdr ||
		ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
		ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
		ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
		ehdr->e_ident[EI_MAG3] != ELFMAG3) {
		assert(false && "invalid usertest file");
	}

	usize in_mem_size = 0;
	usize base = 0;
	bool base_found = false;
	for (u16 i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64PHdr* phdr = (const Elf64PHdr*) ((usize) ehdr + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type == PT_LOAD) {
			if (!base_found) {
				base = phdr->p_vaddr;
				base_found = true;
			}
			in_mem_size += ALIGNUP(phdr->p_memsz, phdr->p_align);
		}
	}

	Task* test_user = arch_create_user_task("basic", NULL, NULL, NULL);

	void* mem;
	void* user_mem = vm_user_alloc_backed(
		test_user,
		ALIGNUP(in_mem_size, PAGE_SIZE) / PAGE_SIZE,
		PF_READ | PF_WRITE | PF_EXEC | PF_USER, &mem);
	assert(user_mem);
	memset(mem, 0, ALIGNUP(in_mem_size, PAGE_SIZE));

	for (u16 i = 0; i < ehdr->e_phnum; ++i) {
		const Elf64PHdr* phdr = (const Elf64PHdr*) ((usize) ehdr + ehdr->e_phoff + i * ehdr->e_phentsize);
		if (phdr->p_type == PT_LOAD) {
			const void* src = (const void*) ((usize) ehdr + phdr->p_offset);
			memcpy((void*) ((usize) mem + (phdr->p_vaddr - base)), src, phdr->p_filesz);
		}
	}

	vm_user_dealloc_kernel(mem, ALIGNUP(in_mem_size, PAGE_SIZE) / PAGE_SIZE);

	void (*user_fn)() = (void (*)()) ((usize) user_mem + (ehdr->e_entry - base));

	arch_set_user_task_fn(test_user, user_fn);

	void* flags = enter_critical();
	//sched_queue_task(test_user);

	leave_critical(flags);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
