#include "acpi/acpi.hpp"
#include "acpi/pci.hpp"
#include "arch/cpu.hpp"
#include "dev/fb/fb_dev.hpp"
#include "dev/net/nic/nic.hpp"
#include "dev/pci.hpp"
#include "exe/elf_loader.hpp"
#include "fs/tar.hpp"
#include "fs/vfs.hpp"
#include "qacpi/context.hpp"
#include "qacpi/ns.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"

// 9a49 == uhd
// a0c8 == audio

struct KernelStdoutVNode : public VNode {
	FsStatus write(const void* data, usize size, usize offset) override {
		if (offset) {
			return FsStatus::Unsupported;
		}

		print(kstd::string_view {static_cast<const char*>(data), size});

		return FsStatus::Success;
	}
};

[[noreturn, gnu::used]] void kmain(const void* initrd, usize initrd_size) {
    println("[kernel]: entered kmain");

	fb_dev_register_boot_fb();

#if ARCH_X86_64
	println("[kernel]: acpi init...");
	pci::acpi_init();
	println("[kernel]: qacpi init");
	acpi::qacpi_init();
	println("[kernel]: pci acpi init");
	acpi::pci_init();
	println("[kernel]: pci enumerate");
	pci::acpi_enumerate();

	kstd::vector<qacpi::NamespaceNode*> nodes;
	nodes.push(acpi::GLOBAL_CTX->get_root());
	while (!nodes.is_empty()) {
		auto node = nodes.pop().value();

		qacpi::EisaId id {"PNP0C0A"};
		if (auto hid = node->get_child("_HID")) {
			auto& obj = hid->get_object();

			if (auto integer = obj->get<uint64_t>()) {
				if (*integer == id.encode()) {
					println("found battery");

					auto ret = qacpi::ObjectRef::empty();
					auto status = acpi::GLOBAL_CTX->evaluate(node, "_BST", ret);
					if (status != qacpi::Status::Success) {
						println("failed evaluate battery, status: ", qacpi::status_to_str(status));
					}
					else {
						auto state = acpi::GLOBAL_CTX->get_package_element(ret, 0)->get_unsafe<uint64_t>();
						auto present = acpi::GLOBAL_CTX->get_package_element(ret, 1)->get_unsafe<uint64_t>();
						auto remaining = acpi::GLOBAL_CTX->get_package_element(ret, 2)->get_unsafe<uint64_t>();
						if (!present) {
							println("battery has no present rate");
						}
						else {
							auto remaining_h = remaining / present;
							println("battery remaining h: ", remaining_h);
							println("battery present rate: ", present, " remaining: ", remaining);
						}
					}
				}
			}
		}

		for (size_t i = 0; i < node->get_child_count(); ++i) {
			nodes.push(node->get_children()[i]);
		}
	}

	//println("[kernel]: done");
#endif

	println("[kernel]: waiting for network");
	nics_wait_ready();

	tar_initramfs_init(initrd, initrd_size);

	IrqGuard guard {};
	auto cpu = get_current_thread()->cpu;

	kstd::shared_ptr<VNode> stdout_vnode {kstd::make_shared<KernelStdoutVNode>()};
	auto stderr_vnode = stdout_vnode;

	auto* user_process = new Process {"user process", true, EmptyHandle {}, std::move(stdout_vnode), std::move(stderr_vnode)};

	auto root = INITRD_VFS->get_root();
	auto bin = root->lookup("bin");
	auto desktop_file = bin->lookup("desktop");
	auto elf_result = elf_load(user_process, desktop_file.data());
	assert(elf_result);

	auto user_arg = reinterpret_cast<void*>(1234);
	auto* user_thread = new Thread {"user thread", cpu, user_process, elf_result.value().entry, user_arg};

	println("[kernel]: launching init");
	cpu->scheduler.queue(user_thread);
	cpu->scheduler.block();
	panic("returned to kmain");
}
