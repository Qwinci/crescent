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
#include "mem/allocator.h"

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

	Process* test_user_process = process_new_user();
	Task* test_user = arch_create_user_task(test_user_process, "basic", NULL, NULL);
	assert(test_user);

	void* mem;
	void* user_mem = vm_user_alloc_backed(
		test_user_process,
		ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE + 100,
		PF_READ | PF_WRITE | PF_EXEC | PF_USER, &mem);
	assert(user_mem);
	memset(mem, 0, ALIGNUP(info.mem_size, PAGE_SIZE) + 100 * PAGE_SIZE);

	LoadedElf loaded = elf_load(info, mem, user_mem);
	elf_protect(info, mem, test_user->map, true);

	vm_user_dealloc_kernel(mem, ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE + 100);

	void (*user_fn)(void*) = (void (*)(void*)) loaded.entry;

	arch_set_user_task_fn(test_user, user_fn);

	//tty_init();

	ACTIVE_INPUT_TASK = test_user;
	ThreadHandle* h = kmalloc(sizeof(ThreadHandle));
	h->task = test_user;
	h->exited = false;
	handle_tab_insert(&test_user_process->handle_table, h, HANDLE_TYPE_THREAD);
	test_user->event_queue.notify_target = test_user;
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(test_user);
	arch_ipl_set(old);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
