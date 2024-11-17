#include "acpi/pci.hpp"
#include "arch/cpu.hpp"
#include "dev/fb/fb_dev.hpp"
#include "dev/net/nic/nic.hpp"
#include "dev/net/tcp.hpp"
#include "exe/elf_loader.hpp"
#include "fs/tar.hpp"
#include "fs/vfs.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"

#ifdef __x86_64__
#include "acpi/acpi.hpp"
#include "acpi/events.hpp"
#include "dev/pci.hpp"
#include "dev/qemu/fw_cfg_x86.hpp"

#include "qacpi/context.hpp"
#include "qacpi/ns.hpp"

#endif

// 9a49 == uhd
// a0c8 == audio

struct KernelStdoutVNode : public VNode {
	KernelStdoutVNode() : VNode {FileFlags::None, false} {}

	FsStatus write(const void* data, usize& size, usize offset) override {
		if (offset) {
			return FsStatus::Unsupported;
		}

		print(kstd::string_view {static_cast<const char*>(data), size});

		return FsStatus::Success;
	}
};

struct NetworkLogSink : LogSink {
	explicit NetworkLogSink(u32 ip) {
		sock = tcp_socket_create(0);
		AnySocketAddress server_addr {
			.ipv4 {
				.generic {
					.type = SOCKET_ADDRESS_TYPE_IPV4
				},
				.ipv4 = ip,
				.port = 8989
			}
		};
		if (sock->connect(server_addr) != 0) {
			sock = nullptr;
			return;
		}
		assert(sock->send("network logging connected\n", sizeof("network logging connected\n") - 1) == 0);

		IrqGuard guard {};
		LOG.lock()->register_sink(this);
	}

	void write(kstd::string_view str) override {
		sock->send(str.data(), str.size());
	}

	kstd::shared_ptr<Socket> sock;
};

void print_mem() {
	println("[kernel]: total memory: ", pmalloc_get_total_mem() / 1024 / 1024, "MB, reserved: ", pmalloc_get_reserved_mem() / 1024 / 1024, "MB");
	println("[kernel]: used memory: ", pmalloc_get_used_mem() / 1024 / 1024, "MB (", pmalloc_get_used_mem() / 1024, "KB)");
}

[[noreturn, gnu::used]] void kmain(const void* initrd, usize initrd_size) {
    println("[kernel]: entered kmain");
	print_mem();

#if ARCH_X86_64
	println("[kernel]: acpi init...");
	pci::acpi_init();
	println("[kernel]: qacpi init");
	acpi::qacpi_init();
	acpi::events_init();
	qemu_fw_cfg_init();
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
						auto state = acpi::GLOBAL_CTX->get_pkg_element(ret, 0)->get_unsafe<uint64_t>();
						auto present = acpi::GLOBAL_CTX->get_pkg_element(ret, 1)->get_unsafe<uint64_t>();
						auto remaining = acpi::GLOBAL_CTX->get_pkg_element(ret, 2)->get_unsafe<uint64_t>();
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

	fb_dev_register_boot_fb();

	println("[kernel]: waiting for network");
	nics_wait_ready();

	// 192.168.1.102
	//u32 log_ip = 192 | 168 << 8 | 1 << 16 | 102 << 24;
	// 10.0.2.2
	u32 log_ip = 10 | 0 << 8 | 2 << 16 | 2 << 24;
	auto netlog = new NetworkLogSink {log_ip};
	if (!netlog->sock) {
		delete netlog;
	}

	tar_initramfs_init(initrd, initrd_size);

	IrqGuard guard {};
	auto cpu = get_current_thread()->cpu;

	kstd::shared_ptr<VNode> stdout_vnode {kstd::make_shared<KernelStdoutVNode>()};
	auto stderr_vnode = stdout_vnode;

	auto stdout_file = kstd::make_shared<OpenFile>(std::move(stdout_vnode));
	auto stderr_file = kstd::make_shared<OpenFile>(std::move(stderr_vnode));

	auto* user_process = new Process {"user process", true, EmptyHandle {}, std::move(stdout_file), std::move(stderr_file)};

	auto root = INITRD_VFS->get_root();
	auto bin = root->lookup("bin");
	auto desktop_file = bin->lookup("desktop");
	auto elf_result = elf_load(user_process, desktop_file.data());
	assert(elf_result);

	SysvInfo sysv_info {
		.ld_entry = reinterpret_cast<usize>(elf_result.value().entry),
		.exe_entry = reinterpret_cast<usize>(elf_result.value().entry),
		.ld_base = elf_result.value().base,
		.exe_phdrs_addr = elf_result.value().phdrs_addr,
		.exe_phdr_count = elf_result.value().phdr_count,
		.exe_phdr_size = elf_result.value().phdr_size
	};

	auto* user_thread = new Thread {"user thread", cpu, user_process, sysv_info};

	println("[kernel]: launching init");
	print_mem();
	cpu->scheduler.queue(user_thread);
	cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
	cpu->scheduler.block();
	panic("returned to kmain");
}
