#include "arch/interrupts.h"
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
#ifdef CONFIG_TEST
#include "utils/test.h"
#endif

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");

#ifdef CONFIG_TEST
	run_tests();
#endif

	pci_init();

	//Module user_file = x86_module_get("basic");
	Module user_file = x86_module_get("user_tty");

	ElfInfo info = elf_get_info(user_file.base);

	Task* test_user = arch_create_user_task("basic", NULL, NULL, NULL, true);
	assert(test_user);

	void* mem;
	void* user_mem = vm_user_alloc_backed(
		test_user,
		ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE + 100,
		PF_READ | PF_WRITE | PF_EXEC | PF_USER, &mem);
	assert(user_mem);
	memset(mem, 0, ALIGNUP(info.mem_size, PAGE_SIZE) + 100 * PAGE_SIZE);

	LoadedElf loaded = elf_load(info, mem, user_mem);
	elf_protect(info, mem, test_user->map, true);

	vm_user_dealloc_kernel(mem, ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE + 100);

	void (*user_fn)() = (void (*)()) loaded.entry;

	arch_set_user_task_fn(test_user, user_fn);

	//tty_init();

	ACTIVE_INPUT_TASK = test_user;
	test_user->event_queue.notify_target = test_user;
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(test_user);
	arch_ipl_set(old);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
