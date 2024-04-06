#include "acpi/acpi.hpp"
#include "acpi/sleep.hpp"
#include "arch/cpu.hpp"
#include "arch/x86/mod.hpp"
#include "dev/fb/fb.hpp"
#include "dev/fb/fb_dev.hpp"
#include "dev/pci.hpp"
#include "exe/elf_loader.hpp"
#include "fs/tar.hpp"
#include "fs/vfs.hpp"
#include "mem/mem.hpp"
#include "mem/register.hpp"
#include "qacpi/context.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"

[[noreturn, gnu::used]] void kmain(const void* initrd, usize initrd_size) {
    println("[kernel]: entered kmain");

	fb_dev_register_boot_fb();

#if ARCH_X86_64
	//println("[kernel]: enumerating pci devices...");
	pci::acpi_enumerate();
	//println("[kernel]: done");
#endif

	acpi::qacpi_init();
	acpi::enter_sleep_state(acpi::SleepState::S5);

	tar_initramfs_init(initrd, initrd_size);

	IrqGuard guard {};
	auto cpu = get_current_thread()->cpu;

	auto* user_process = new Process {"user process", true};

	auto root = INITRD_VFS->get_root();
	auto bin = root->lookup("bin");
	auto desktop_file = bin->lookup("desktop");
	auto elf_result = elf_load(user_process, desktop_file);
	assert(elf_result);

	auto user_arg = reinterpret_cast<void*>(1234);
	auto* user_thread = new Thread {"user thread", cpu, user_process, elf_result.value().entry, user_arg};

	cpu->scheduler.queue(user_thread);
	cpu->scheduler.block();
	panic("returned to kmain");
}
