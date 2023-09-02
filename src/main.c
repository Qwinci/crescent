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
#include "mem/allocator.h"
#include "fs/tar.h"
#include "fs/vfs.h"

[[noreturn]] void kmain() {
	kprintf("[kernel]: entered main\n");

#ifdef CONFIG_TEST
	run_tests();
#endif
	pci_init();

	Vfs* vfs = VFS_LIST;
	if (vfs) {
		VNode* root = vfs->get_root(vfs);
		void* state = root->begin_read_dir(root);
		DirEntry dir;
		kprintf("---------------------\n");
		while (root->read_dir(root, state, &dir)) {
			kprintf("%s\n", dir.name);

			VNode* entry = root->lookup(root, str_new_with_len(dir.name, dir.name_len));
			assert(entry);

			if (entry->type == VNODE_FILE) {
				kprintf("content: \n");

				Stat stat;
				assert(entry->stat(entry, &stat));

				char* buf = kmalloc(stat.size);
				assert(buf);
				assert(entry->read(entry, buf, 0, stat.size));

				kprintf("\t%s\n", buf);
				kfree(buf, stat.size);
			}

			entry->release(entry);
		}
		kprintf("---------------------\n");
		root->end_read_dir(root, state);

		root->release(root);
	}

	tar_initramfs_init();

	Module user_file = arch_get_module("user_tty");

	ElfInfo info = elf_get_info(user_file.base);

	Process* test_user_process = process_new_user();
	Task* test_user = arch_create_user_task(test_user_process, "basic", NULL, NULL);
	assert(test_user);

	void* mem;
	void* user_mem = vm_user_alloc_backed(
		test_user_process,
		NULL,
		ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE,
		PF_READ | PF_WRITE | PF_EXEC | PF_USER, &mem);
	assert(user_mem);
	memset(mem, 0, ALIGNUP(info.mem_size, PAGE_SIZE));

	LoadedElf loaded = elf_load(info, mem, user_mem);
	elf_protect(info, mem, test_user->map, true);

	vm_user_dealloc_kernel(mem, ALIGNUP(info.mem_size, PAGE_SIZE) / PAGE_SIZE);

	void (*user_fn)(void*) = (void (*)(void*)) loaded.entry;

	arch_set_user_task_fn(test_user, user_fn);

	//tty_init();

	ACTIVE_INPUT_TASK = test_user;
	ThreadHandle* h = kmalloc(sizeof(ThreadHandle));
	h->task = test_user;
	h->exited = false;
	memset(&h->lock, 0, sizeof(Mutex));
	handle_tab_insert(&test_user_process->handle_table, h, HANDLE_TYPE_THREAD);
	test_user->event_queue.notify_target = test_user;

	process_add_thread(test_user_process, test_user);

	Ipl old = arch_ipl_set(IPL_CRITICAL);
	sched_queue_task(test_user);
	arch_ipl_set(old);

	sched_block(TASK_STATUS_WAITING);
	while (true) {
		arch_hlt();
	}
}
