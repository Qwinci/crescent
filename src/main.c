#include "arch/misc.h"
#include "arch/x86/mod.h"
#include "assert.h"
#include "dev/pci_enum.h"
#include "exe/elf_loader.h"
#include "mem/page.h"
#include "mem/utils.h"
#include "mem/vm.h"
#include "sched/sched.h"
#include "stdio.h"
#include "string.h"
#include "tty/tty.h"

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");

	pci_init();

	Module user_file = x86_module_get("basic");

	ElfInfo info = elf_get_info(user_file.base);

	Task* test_user = arch_create_user_task("basic", NULL, NULL, NULL, true);

	void* mem;
	void* user_mem = vm_user_alloc_backed(
		test_user,
		ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE,
		PF_READ | PF_WRITE | PF_EXEC | PF_USER, &mem);
	assert(user_mem);
	memset(mem, 0, ALIGNUP(info.mem_size, PAGE_SIZE));

	LoadedElf loaded = elf_load(info, mem, user_mem);
	elf_protect(info, mem, test_user->map, true);

	vm_user_dealloc_kernel(mem, ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE);

	void (*user_fn)() = (void (*)()) loaded.entry;

	arch_set_user_task_fn(test_user, user_fn);

	tty_init();

	void* flags = enter_critical();
	sched_queue_task(test_user);
	leave_critical(flags);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
