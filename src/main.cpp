#include "acpi/acpi.hpp"
#include "arch/cpu.hpp"
#include "dev/fb/fb_dev.hpp"
#include "dev/pci.hpp"
#include "exe/elf_loader.hpp"
#include "fs/tar.hpp"
#include "fs/vfs.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"
#include "qacpi/context.hpp"
#include "qacpi/ns.hpp"

namespace acpi {
	extern qacpi::Context GLOBAL_CTX;
}

[[noreturn, gnu::used]] void kmain(const void* initrd, usize initrd_size) {
    println("[kernel]: entered kmain");

	fb_dev_register_boot_fb();

#if ARCH_X86_64
	//println("[kernel]: enumerating pci devices...");
	pci::acpi_init();
	acpi::qacpi_init();
	pci::acpi_enumerate();

	kstd::vector<qacpi::NamespaceNode*> nodes;
	nodes.push(acpi::GLOBAL_CTX.get_root());
	while (!nodes.is_empty()) {
		auto node = nodes.pop().value();

		qacpi::EisaId id {"PNP0C0A"};
		if (auto hid = node->get_child("_HID")) {
			auto& obj = hid->get_object();
			if (auto integer = obj->get<uint64_t>()) {
				if (*integer == id.encode()) {
					println("found battery");

					auto ret = qacpi::ObjectRef::empty();
					acpi::GLOBAL_CTX.evaluate(node, "_BST", ret);
					auto& ret_pkg = ret->get_unsafe<qacpi::Package>();
					auto state = ret_pkg.data->elements[0]->get_unsafe<uint64_t>();
					auto present = ret_pkg.data->elements[1]->get_unsafe<uint64_t>();
					auto remaining = ret_pkg.data->elements[2]->get_unsafe<uint64_t>();

					auto remaining_h = remaining / present;
					println("battery remaining h: ", remaining_h);
					println("battery present rate: ", present, " remaining: ", remaining);
				}
			}
		}

		for (size_t i = 0; i < node->get_child_count(); ++i) {
			nodes.push(node->get_children()[i]);
		}
	}

	//println("[kernel]: done");
#endif

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
