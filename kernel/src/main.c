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
#include "fs/posix_rootfs.h"
#include "fs/tar.h"
#include "fs/vfs.h"
#include "mem/allocator.h"
#include "sys/fs.h"
#include "utils/cmdline.h"
#include "utils/elf.h"

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

	GenericDevice* root_device = dev_get(parsed_cmdline.posix_root.data, parsed_cmdline.posix_root.len);
	if (!root_device) {
		panic("failed to open root device '%.*s'\n", (int) parsed_cmdline.posix_root.len, parsed_cmdline.posix_root.data);
	}
	if (root_device->type != DEVICE_TYPE_PARTITION) {
		panic("device '%.*s' is not a partition\n", (int) parsed_cmdline.posix_root.len, parsed_cmdline.posix_root.data);
	}
	PartitionDev* root_part = container_of(root_device, PartitionDev, generic);

	kprintf("[kernel][fs]: initializing posix rootfs union 'posix_rootfs' using '%.*s'\n", (int) parsed_cmdline.posix_root.len, parsed_cmdline.posix_root.data);
	posix_rootfs_init(root_part->vfs);

	LoadedElf init_res;
	Process* init_process = process_new_user();
	assert(init_process);
	//Task* init_task = arch_create_user_task(init_process, "init", NULL, NULL);

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

	// todo don't hardcode this
	const char* interp_path = "posix_rootfs/lib/ld.so";
	VNode* interp_vnode;
	if ((status = kernel_fs_open(interp_path, &interp_vnode)) != 0) {
		panic("failed to open interp file '%s': %d\n", interp_path, status);
	}

	usize interp_base;
	if ((status = elf_load_from_file(init_process, interp_vnode, &init_res, false, &interp_base)) != 0) {
		panic("failed to load init elf, status: %d\n", status);
	}
	void* user_phdrs;
	usize user_phdr_count;
	usize user_entry;
	if ((status = elf_load_user_phdrs(init_process, init_vnode, &user_phdrs, &user_phdr_count, &user_entry)) != 0) {
		panic("failed to load user phdrs, status: %d\n", status);
	}

	Str arg0 = str_new("ext2_p0/libc_test_program");
	Str args[] = {arg0};
	SysvAux aux[] = {
		{.type = AT_PHDR, .value = (size_t) user_phdrs},
		{.type = AT_PHENT, .value = sizeof(Elf64PHdr)},
		{.type = AT_PHNUM, .value = user_phdr_count},
		{.type = AT_PAGESZ, .value = PAGE_SIZE},
		{.type = AT_ENTRY, .value = user_entry}
	};
	SysvTaskInfo info = {
		.aux = aux,
		.aux_len = sizeof(aux) / sizeof(*aux),
		.args = args,
		.args_len = sizeof(args) / sizeof(*args),
		.env = NULL,
		.env_len = 0,
		.fn = (void (*)()) init_res.entry
	};
	Task* init_task = arch_create_sysv_user_task(init_process, "init", &info);

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
