#include "arch/interrupts.h"
#include "arch/misc.h"
#include "arch/mod.h"
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
#include "fs/tar.h"
#include "fs/vfs.h"
#include "mem/allocator.h"
#include "sys/fs.h"
#include "utils/cmdline.h"

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");
	const char* cmdline = arch_get_kernel_cmdline();
	kprintf("[kernel]: cmdline: %s\n", cmdline);
	KernelCmdline parsed_cmdline = parse_kernel_cmdline(cmdline);

#ifdef CONFIG_TEST
	run_tests();
#endif
	pci_init();

	tar_initramfs_init();

	LoadedElf init_res;
	Process* init_process = process_new_user();
	assert(init_process);
	Task* init_task = arch_create_user_task(init_process, "init", NULL, NULL);
	VNode* init_vnode;

	char* null_terminated_init = kmalloc(parsed_cmdline.init.len + 1);
	assert(null_terminated_init);
	memcpy(null_terminated_init, parsed_cmdline.init.data, parsed_cmdline.init.len);
	null_terminated_init[parsed_cmdline.init.len] = 0;

	int status;
	if ((status = kernel_fs_open(null_terminated_init, &init_vnode)) != 0) {
		panic("failed to open init file '%s': %d\n", null_terminated_init, status);
	}
	kfree(null_terminated_init, parsed_cmdline.init.len + 1);

	if ((status = elf_load_from_file(init_task, init_vnode, &init_res)) != 0) {
		panic("failed to load init elf, status: %d\n", status);
	}

	void (*init_entry)(void*) = (void (*)(void*)) init_res.entry;

	arch_set_user_task_fn(init_task, init_entry);

	ACTIVE_INPUT_TASK = init_task;
	ThreadHandle* h = kmalloc(sizeof(ThreadHandle));
	assert(h);
	h->refcount = 1;
	h->task = init_task;
	h->exited = false;
	memset(&h->lock, 0, sizeof(Mutex));
	init_task->tid = handle_tab_insert(&init_process->handle_table, h, HANDLE_TYPE_THREAD);
	init_task->event_queue.notify_target = init_task;

	process_add_thread(init_process, init_task);

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(init_task);
	arch_ipl_set(old);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
