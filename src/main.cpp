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
#include "utils/kgdb.hpp"
#include "sys/event_queue.hpp"

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

		print(Color::White, kstd::string_view {static_cast<const char*>(data), size}, Color::Reset);

		return FsStatus::Success;
	}
};

struct KernelStdinVNode : public VNode {
	KernelStdinVNode() : VNode {FileFlags::None, false} {
		auto cpu = get_current_thread()->cpu;

		struct Data {
			KernelStdinVNode* file;
		};

		auto data = new Data {this};

		auto stdio_poller = new Thread {
			"stdio poller",
			cpu,
			&*KERNEL_PROCESS,
			[](void* arg) {
				auto* data = static_cast<Data*>(arg);
				while (true) {
					InputEvent event {};
					if (!GLOBAL_EVENT_QUEUE.consume(event)) {
						GLOBAL_EVENT_QUEUE.produce_event.wait();
						continue;
					}
					if (event.type != EVENT_TYPE_KEY || !event.key.pressed) {
						continue;
					}

					if (!data->file) {
						IrqGuard irq_guard {};
						get_current_thread()->cpu->scheduler.exit_thread(0);
					}

					data->file->input_events.push(event);
					data->file->input_event.signal_one();
					data->file->poll_event.signal_one_if_not_pending();
				}
			},
			data};
		node = &data->file;
		stdio_poller->pin_level = true;
		stdio_poller->pin_cpu = true;
		cpu->scheduler.queue(stdio_poller);
	}

	~KernelStdinVNode() override {
		*node = nullptr;
	}

	int index = 0;
	KernelStdinVNode** node;

	FsStatus read(void* data, usize& size, usize offset) override {
		if (!size) {
			return FsStatus::Success;
		}

		auto ptr = static_cast<char*>(data);

		usize remaining = size;
		while (true) {
			if (input_events.is_empty()) {
				input_event.wait();
				continue;
			}

			auto event = input_events[0];
			input_events.remove(0);

			if (event.key.code >= SCANCODE_A && event.key.code <= SCANCODE_Z) {
				char c = static_cast<char>('a' + (event.key.code - SCANCODE_A));
				ptr[size - remaining] = c;

				--remaining;
				if (!remaining) {
					break;
				}
			}
			else if (event.key.code == SCANCODE_RETURN) {
				ptr[size - remaining] = '\n';
				--remaining;
				if (!remaining) {
					break;
				}
			}
			else if (event.key.code == SCANCODE_SPACE && event.key.pressed) {
				ptr[size - remaining] = ' ';
				--remaining;
				if (!remaining) {
					break;
				}
			}
			else {
				ptr[size - remaining] = '?';
				--remaining;
				if (!remaining) {
					break;
				}
			}
		}

		return FsStatus::Success;
	}

	FsStatus poll(PollEvent& events) override {
		if (!input_events.is_empty()) {
			events |= PollEvent::In;
		}
		return FsStatus::Success;
	}

	kstd::vector<InputEvent> input_events {};
	Event input_event {};
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
		usize size = sizeof("network logging connected\n") - 1;
		assert(sock->send("network logging connected\n", size) == 0);

		IrqGuard guard {};
		LOG.lock()->register_sink(this);
	}

	void write(kstd::string_view str) override {
		usize size = str.size();
		sock->send(str.data(), size);
	}

	kstd::shared_ptr<Socket> sock;
};

void print_mem() {
	//println("[kernel]: total memory: ", pmalloc_get_total_mem() / 1024 / 1024, "MB, reserved: ", pmalloc_get_reserved_mem() / 1024 / 1024, "MB");
	//println("[kernel]: used memory: ", pmalloc_get_used_mem() / 1024 / 1024, "MB (", pmalloc_get_used_mem() / 1024, "KB)");
}

[[noreturn, gnu::used]] void kmain(const void* initrd, usize initrd_size) {
   // println("[kernel]: entered kmain");
	print_mem();

#if ARCH_X86_64
	//println("[kernel]: acpi init...");
	pci::acpi_init();
	//println("[kernel]: qacpi init");
	acpi::qacpi_init();
	acpi::events_init();
	qemu_fw_cfg_init();
	//println("[kernel]: pci acpi init");
	acpi::pci_init();
	//println("[kernel]: pci enumerate");
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
					println("evaluating _BST");
					auto status = acpi::GLOBAL_CTX->evaluate(node, "_BST", ret);
					println("bst done");
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

	//println("[kernel]: waiting for network");
	nics_wait_ready();

	// 192.168.1.102
	u32 log_ip = 192 | 168 << 8 | 1 << 16 | 102 << 24;
	// 10.0.2.2
	//u32 log_ip = 10 | 0 << 8 | 2 << 16 | 2 << 24;
	auto netlog = new NetworkLogSink {log_ip};
	if (!netlog->sock) {
		delete netlog;
	}

	kgdb_init();

	tar_initramfs_init(initrd, initrd_size);

	IrqGuard guard {};
	auto cpu = get_current_thread()->cpu;
	//kstd::shared_ptr<VNode> stdin_vnode {kstd::make_shared<KernelStdinVNode>()};
	kstd::shared_ptr<VNode> stdout_vnode {kstd::make_shared<KernelStdoutVNode>()};
	auto stderr_vnode = stdout_vnode;

	//auto stdin_file = kstd::make_shared<OpenFile>(std::move(stdin_vnode));
	auto stdin_file = EmptyHandle {};
	auto stdout_file = kstd::make_shared<OpenFile>(std::move(stdout_vnode));
	auto stderr_file = kstd::make_shared<OpenFile>(std::move(stderr_vnode));

	auto* user_process = new Process {"user process", true, std::move(stdin_file), std::move(stdout_file), std::move(stderr_file)};

	//auto desktop_file = vfs_lookup(INITRD_VFS->get_root(), "bin/desktop");
	//auto desktop_file = vfs_lookup(INITRD_VFS->get_root(), "usr/bin/bash");
	//auto desktop_file = vfs_lookup(INITRD_VFS->get_root(), "bin/console");

	//auto init_file = vfs_lookup(INITRD_VFS->get_root(), "bin/desktop");

	auto init_file = vfs_lookup(INITRD_VFS->get_root(), "bin/init");
	assert(init_file);

	auto elf_result = elf_load(user_process, init_file.data());
	assert(elf_result);

	auto ld_file = vfs_lookup(INITRD_VFS->get_root(), "usr/lib/libc.so");
	assert(ld_file);

	auto ld_elf_result = elf_load(user_process, ld_file.data());
	assert(ld_elf_result);

	SysvInfo sysv_info {
		.ld_entry = reinterpret_cast<usize>(ld_elf_result.value().entry),
		.exe_entry = reinterpret_cast<usize>(elf_result.value().entry),
		.ld_base = ld_elf_result.value().base,
		.exe_phdrs_addr = elf_result.value().phdrs_addr,
		.exe_phdr_count = elf_result.value().phdr_count,
		.exe_phdr_size = elf_result.value().phdr_size
	};

	auto* user_thread = new Thread {"init", cpu, user_process, sysv_info};

	println("[kernel]: launching init");
	print_mem();
	cpu->scheduler.queue(user_thread);
	cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
	cpu->scheduler.block();
	panic("returned to kmain");
}
