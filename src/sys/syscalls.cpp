#include "syscalls.hpp"
#include "user_access.hpp"
#include "arch/cpu.hpp"
#include "crescent/devlink.h"
#include "crescent/syscalls.h"
#include "crescent/socket.h"
#include "event_queue.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"
#include "fs/vfs.hpp"
#include "fs/pipe.hpp"
#include "exe/elf_loader.hpp"
#include "service.hpp"
#include "sched/ipc.hpp"
#include "dev/net/tcp.hpp"
#include "dev/net/udp.hpp"
#include "dev/date_time_provider.hpp"

#ifdef __x86_64__
#include "acpi/sleep.hpp"
#include "arch/x86/cpu.hpp"
#include "arch/x86/dev/vmx.hpp"
#elif defined(__aarch64__)
#include "arch/aarch64/dev/psci.hpp"
#include "exe/elf_loader.hpp"
#include "fs/vfs.hpp"
#endif

static usize socket_address_type_to_size(SocketAddressType type) {
	switch (type) {
		case SOCKET_ADDRESS_TYPE_IPC:
			return sizeof(IpcSocketAddress);
		case SOCKET_ADDRESS_TYPE_IPV4:
			return sizeof(Ipv4SocketAddress);
		case SOCKET_ADDRESS_TYPE_IPV6:
			return sizeof(Ipv6SocketAddress);
		default:
			return sizeof(SocketAddress);
	}
}

extern "C" void syscall_handler(SyscallFrame* frame) {
	auto num = *frame->num();
	auto thread = get_current_thread();

	switch (static_cast<CrescentSyscall>(num)) {
		case SYS_THREAD_CREATE:
		{
			if (thread->process->killed) {
				println("[kernel]: cancelling sys_thread_create because process is exiting");
				*frame->ret() = ERR_UNSUPPORTED;
				break;
			}

			kstd::string name;
			usize size = *frame->arg2();
			name.resize_without_null(size);
			if (!UserAccessor(*frame->arg1()).load(name.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}
			auto fn = reinterpret_cast<void (*)(void*)>(*frame->arg3());
			auto arg = reinterpret_cast<void*>(*frame->arg4());

			Cpu* cpu;
			{
				IrqGuard guard {};
				cpu = thread->cpu;
			}

			auto* new_thread = new Thread {name, cpu, thread->process, fn, arg};

			auto descriptor = kstd::make_shared<ThreadDescriptor>(new_thread, 0);
			CrescentHandle handle = thread->process->handles.insert(descriptor);
			new_thread->add_descriptor(descriptor.data());

			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				delete new_thread;
				*frame->ret() = ERR_FAULT;
				break;
			}

			println("[kernel]: create thread ", name);

			IrqGuard guard {};
			cpu->scheduler.queue(new_thread);
			cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);

			*frame->ret() = 0;
			break;
		}
		case SYS_THREAD_EXIT:
		{
			IrqGuard guard {};
			Cpu* cpu = thread->cpu;
			auto status = static_cast<int>(*frame->arg0());
			println("[kernel]: thread ", thread->name, " exited with status ", status);
			cpu->scheduler.exit_thread(status);
		}
		case SYS_PROCESS_CREATE:
		{
			kstd::string path;
			usize path_len = *frame->arg2();
			path.resize_without_null(path_len);
			if (!UserAccessor(*frame->arg1()).load(path.data(), path_len)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			ProcessCreateInfo process_info {};
			if (!UserAccessor(*frame->arg3()).load(process_info)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			kstd::vector<kstd::string> args;
			args.resize(process_info.arg_count);
			bool success = true;
			for (usize i = 0; i < process_info.arg_count; ++i) {
				CrescentStringView view;
				if (!UserAccessor(offset(process_info.args, void*, i * sizeof(CrescentStringView))).load(view)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
				args[i].resize_without_null(view.len);
				if (!UserAccessor(view.str).load(args[i].data(), view.len)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
			}

			if (!success) {
				break;
			}

			auto path_view = path.as_view();
			auto name_start = path_view.rfind('/');
			if (name_start == kstd::string_view::npos) {
				name_start = 0;
			}
			else {
				++name_start;
			}
			auto name = path_view.substr(name_start);

			auto node = vfs_lookup(INITRD_VFS->get_root(), path_view);
			if (!node) {
				*frame->ret() = ERR_NOT_EXISTS;
				break;
			}

			Process* process;
			if (process_info.flags & PROCESS_STD_HANDLES) {
				auto stdin = thread->process->handles.get(process_info.stdin_handle);
				if (!stdin || !stdin->get<kstd::shared_ptr<OpenFile>>()) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				auto stdout = thread->process->handles.get(process_info.stdout_handle);
				if (!stdout || !stdout->get<kstd::shared_ptr<OpenFile>>()) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				auto stderr = thread->process->handles.get(process_info.stderr_handle);
				if (!stderr || !stderr->get<kstd::shared_ptr<OpenFile>>()) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}

				process = new Process {name, true, std::move(stdin).value(), std::move(stdout).value(), std::move(stderr).value()};
			}
			else {
				auto stdin = thread->process->handles.get(STDIN_HANDLE).value();
				auto stdout = thread->process->handles.get(STDOUT_HANDLE).value();
				auto stderr = thread->process->handles.get(STDERR_HANDLE).value();

				process = new Process {name, true, std::move(stdin), std::move(stdout), std::move(stderr)};
			}

			auto elf_result = elf_load(process, node.data());
			if (!elf_result) {
				switch (elf_result.error()) {
					case ElfLoadError::Invalid:
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					case ElfLoadError::NoMemory:
						*frame->ret() = ERR_NO_MEM;
						break;
				}
				break;
			}

			auto ld_file = vfs_lookup(INITRD_VFS->get_root(), "usr/lib/libc.so");
			assert(ld_file);

			auto ld_elf_result = elf_load(process, ld_file.data());
			assert(ld_elf_result);

			SysvInfo sysv_info {
				.ld_entry = reinterpret_cast<usize>(ld_elf_result.value().entry),
				.exe_entry = reinterpret_cast<usize>(elf_result.value().entry),
				.ld_base = ld_elf_result.value().base,
				.exe_phdrs_addr = elf_result.value().phdrs_addr,
				.exe_phdr_count = elf_result.value().phdr_count,
				.exe_phdr_size = elf_result.value().phdr_size
			};

			auto descriptor = kstd::make_shared<ProcessDescriptor>(process, 0);
			CrescentHandle handle = thread->process->handles.insert(descriptor);
			process->add_descriptor(descriptor.data());

			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				delete process;
				*frame->ret() = ERR_FAULT;
				break;
			}

			IrqGuard irq_guard {};
			auto cpu = get_current_thread()->cpu;
			auto* new_thread = new Thread {
				name,
				cpu,
				process,
				sysv_info
			};
			cpu->scheduler.queue(new_thread);
			cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);

			*frame->ret() = 0;
			break;
		}
		case SYS_PROCESS_EXIT:
		{
			auto status = static_cast<int>(*frame->arg0());
			println("[kernel]: process ", thread->process->name, " exited with status ", status);
			IrqGuard guard {};
			Cpu* cpu = thread->cpu;
			cpu->scheduler.exit_process(status);
		}
		case SYS_KILL:
		{
			CrescentHandle user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);
			if (!handle) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			IrqGuard irq_guard {};
			if (auto proc_desc = handle->get<kstd::shared_ptr<ProcessDescriptor>>()) {
				*frame->ret() = 0;

				auto guard = (*proc_desc)->process.lock();
				if (!guard) {
					break;
				}
				(*guard)->killed = true;
				(*guard)->exit(-1, (*proc_desc).data());
			}
			else if (auto thread_desc = handle->get<kstd::shared_ptr<ThreadDescriptor>>()) {
				*frame->ret() = 0;

				auto guard = (*thread_desc)->thread.lock();
				if (!guard) {
					break;
				}
				(*guard)->exited = true;
				(*guard)->exit(-1, (*thread_desc).data());
			}
			else {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			break;
		}
		case SYS_GET_STATUS:
		{
			CrescentHandle user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);
			if (!handle) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			IrqGuard irq_guard {};
			if (auto proc_desc = handle->get<kstd::shared_ptr<ProcessDescriptor>>()) {
				auto guard = (*proc_desc)->process.lock();
				if (!guard) {
					*frame->ret() = (*proc_desc)->exit_status;
					break;
				}
				*frame->ret() = ERR_TRY_AGAIN;
			}
			else if (auto thread_desc = handle->get<kstd::shared_ptr<ThreadDescriptor>>()) {
				auto guard = (*thread_desc)->thread.lock();
				if (!guard) {
					*frame->ret() = (*thread_desc)->exit_status;
					break;
				}
				*frame->ret() = ERR_TRY_AGAIN;
			}
			else {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			break;
		}
		case SYS_GET_THREAD_ID:
		{
			*frame->ret() = thread->thread_id;
			break;
		}
		case SYS_SLEEP:
		{
			auto ns = *frame->arg0();
			if (ns > SCHED_MAX_SLEEP_US * NS_IN_US) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			thread->sleep_for(ns);
			*frame->ret() = 0;
			break;
		}
		case SYS_GET_TIME:
		{
			u64 now;
			{
				IrqGuard irq_guard {};
				auto guard = CLOCK_SOURCE.lock_read();
				now = (*guard)->get_ns();
			}

			if (!UserAccessor(*frame->arg0()).store(now)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_GET_DATE_TIME:
		{
			IrqGuard irq_guard {};
			auto guard = DATE_TIME_PROVIDER.lock();
			if (!*guard) {
				*frame->ret() = ERR_UNSUPPORTED;
				break;
			}

			CrescentDateTime res {};
			auto status = (*guard)->get_date(res);
			if (status == 0) {
				if (!UserAccessor(*frame->arg0()).store(res)) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			*frame->ret() = status;

			break;
		}
		case SYS_SYSLOG:
		{
			kstd::string str;
			usize size = *frame->arg1();
			str.resize_without_null(size);
			if (!UserAccessor(*frame->arg0()).load(str.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}
			print(str);

			*frame->ret() = 0;
			break;
		}
		case SYS_MAP:
		{
			void* ptr;
			if (!UserAccessor(*frame->arg0()).load(ptr)) {
				*frame->ret() = ERR_FAULT;
				break;
			}
			auto size = *frame->arg1();
			int protection = static_cast<int>(*frame->arg2());

			MemoryAllocFlags flags = MemoryAllocFlags::Backed;
			PageFlags prot {};
			if (protection & CRESCENT_PROT_READ) {
				prot |= PageFlags::Read;
			}
			if (protection & CRESCENT_PROT_WRITE) {
				prot |= PageFlags::Write;
			}
			if (protection & CRESCENT_PROT_EXEC) {
				prot |= PageFlags::Execute;
			}

			auto addr = thread->process->allocate(ptr, size, prot, flags, nullptr);
			if (!addr) {
				*frame->ret() = ERR_NO_MEM;
				break;
			}
			if (!UserAccessor(*frame->arg0()).store(addr)) {
				thread->process->free(addr, size);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;
			break;
		}
		case SYS_UNMAP:
		{
			auto ptr = *frame->arg0();
			auto size = *frame->arg1();

			thread->process->free(ptr, size);
			*frame->ret() = 0;
			break;
		}
		case SYS_DEVLINK:
		{
			DevLink link {};

			DevLinkRequest req {};

			if (!UserAccessor(*frame->arg0()).load(link) ||
				!UserAccessor(reinterpret_cast<usize>(link.request)).load(req)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			switch (req.type) {
				case DevLinkRequestType::GetDevices:
				{
					auto type = req.data.get_devices.type;
					if (type >= CrescentDeviceType::Max) {
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					}

					auto guard = USER_DEVICES[static_cast<int>(type)]->lock();
					usize needed_hdr = sizeof(DevLinkResponse) +
									   sizeof(const char*) * guard->size() +
									   sizeof(size_t*) * guard->size();

					kstd::vector<u8> resp_buf;
					resp_buf.resize(needed_hdr);

					for (auto& device : *guard) {
						auto old = resp_buf.size();
						resp_buf.resize(resp_buf.size() + device->name.size());
						memcpy(resp_buf.data() + old, device->name.data(), device->name.size());
					}

					auto response_buf = reinterpret_cast<usize>(link.response_buffer);
					auto max_size = kstd::min(link.response_buf_size, resp_buf.size());

					auto* resp = reinterpret_cast<DevLinkResponse*>(resp_buf.data());
					resp->size = resp_buf.size();
					resp->get_devices.device_count = guard->size();
					resp->get_devices.names = reinterpret_cast<const char**>(response_buf + sizeof(DevLinkResponse));
					resp->get_devices.name_sizes = reinterpret_cast<size_t*>(
						response_buf +
						sizeof(DevLinkResponse) +
						sizeof(const char*) * guard->size());

					auto* kernel_names = reinterpret_cast<const char**>(resp_buf.data() + sizeof(DevLinkResponse));
					auto* kernel_sizes = reinterpret_cast<size_t*>(
						resp_buf.data() +
						sizeof(DevLinkResponse) +
						sizeof(const char*) * guard->size());

					usize offset = needed_hdr;
					for (usize i = 0; i < guard->size(); ++i) {
						auto& device = (*guard)[i];
						kernel_names[i] = reinterpret_cast<const char*>(response_buf + offset);
						kernel_sizes[i] = device->name.size() - 1;
						offset += device->name.size();
					}

					int ret;
					if (resp_buf.size() > max_size) {
						ret = ERR_BUFFER_TOO_SMALL;
					}
					else {
						ret = 0;
					}

					if (!UserAccessor(response_buf).store(resp_buf.data(), max_size)) {
						ret = ERR_FAULT;
					}

					*frame->ret() = ret;

					break;
				}
				case DevLinkRequestType::OpenDevice:
				{
					if (req.data.open_device.device_len > 1024) {
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					}

					kstd::string name;
					name.resize_without_null(req.data.open_device.device_len);
					name[req.data.open_device.device_len] = 0;
					if (!UserAccessor(req.data.open_device.device).load(name.data(), req.data.open_device.device_len)) {
						*frame->ret() = ERR_FAULT;
						break;
					}

					bool success = false;

					auto guard = USER_DEVICES[static_cast<int>(req.data.open_device.type)]->lock();
					for (auto& device : *guard) {
						if (device->name == name) {
							if (device->exclusive && device->open_count.load(kstd::memory_order::relaxed) != 0) {
								*frame->ret() = ERR_TRY_AGAIN;
								success = true;
								break;
							}

							auto handle = thread->process->handles.insert(DeviceHandle {device});
							DevLinkResponse resp {
								.size = sizeof(DevLinkResponse),
								.open_device {
									.handle = handle
								}
							};
							if (!UserAccessor(link.response).store(resp)) {
								thread->process->handles.remove(handle);
								*frame->ret() = ERR_FAULT;
								success = true;
								break;
							}

							success = true;
							*frame->ret() = 0;
							break;
						}
					}

					if (!success) {
						*frame->ret() = ERR_NOT_EXISTS;
					}

					break;
				}
				case DevLinkRequestType::Specific:
				{
					if (req.size > DEVLINK_BUFFER_SIZE) {
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					}

					auto handle = thread->process->handles.get(req.handle);
					DeviceHandle* device;
					if (!handle || !(device = handle->get<DeviceHandle>())) {
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					}

					kstd::vector<u8> req_data;
					req_data.resize(req.size);
					if (!UserAccessor(link.request).load(req_data.data(), req_data.size())) {
						*frame->ret() = ERR_FAULT;
						break;
					}

					if (link.response_buf_size < sizeof(DevLinkResponse)) {
						*frame->ret() = ERR_BUFFER_TOO_SMALL;
						break;
					}

					kstd::vector<u8> resp_data;
					auto status = (*device).device->handle_request(req_data, resp_data, link.response_buf_size - sizeof(DevLinkResponse));
					*frame->ret() = status;

					DevLinkResponse resp {
						.size = sizeof(DevLinkResponse) + resp_data.size(),
						.specific = offset(link.response_buffer, void*, sizeof(DevLinkResponse))
					};

					if (status == 0) {
						if (!UserAccessor(link.response_buffer).store(resp)) {
							*frame->ret() = ERR_FAULT;
							break;
						}
						if (!UserAccessor(link.response_buffer + sizeof(DevLinkResponse)).store(resp_data.data(), resp_data.size())) {
							*frame->ret() = ERR_FAULT;
							break;
						}
					}

					break;
				}
			}

			break;
		}
		case SYS_CLOSE_HANDLE:
		{
			auto handle = *frame->arg0();
			if (!thread->process->handles.remove(handle)) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
			}
			else {
				*frame->ret() = 0;
			}
			break;
		}
		case SYS_MOVE_HANDLE:
		{
			CrescentHandle user_handle;
			if (!UserAccessor(*frame->arg0()).load(user_handle)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto handle = thread->process->handles.get(user_handle);
			if (!handle) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			CrescentHandle user_proc_handle = *frame->arg1();
			auto proc_handle = thread->process->handles.get(user_proc_handle);
			if (!proc_handle) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			Spinlock<Process*>* process_lock;
			if (auto proc_desc_ptr = proc_handle->get<kstd::shared_ptr<ProcessDescriptor>>()) {
				process_lock = &proc_desc_ptr->data()->process;
			}
			else {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto copy = std::move(handle.value());
			thread->process->handles.remove(user_handle);

			auto guard = process_lock->lock();
			if (!*guard) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto res_handle = (*guard)->handles.insert(std::move(copy));

			if (!UserAccessor(*frame->arg0()).store(res_handle)) {
				(*guard)->handles.remove(res_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_POLL_EVENT:
		{
			InputEvent event {};
			size_t timeout_ns = *frame->arg1();
			if (timeout_ns != SIZE_MAX && timeout_ns > SCHED_MAX_SLEEP_US * NS_IN_US) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			while (true) {
				auto ret = GLOBAL_EVENT_QUEUE.consume(event);

				if (ret) {
					if (!UserAccessor(*frame->arg0()).store(event)) {
						*frame->ret() = ERR_FAULT;
						break;
					}
					else {
						*frame->ret() = 0;
						break;
					}
				}
				else {
					if (!timeout_ns) {
						*frame->ret() = ERR_TRY_AGAIN;
						break;
					}
					else {
						if (timeout_ns == SIZE_MAX) {
							GLOBAL_EVENT_QUEUE.produce_event.wait();
						}
						else {
							if (!GLOBAL_EVENT_QUEUE.produce_event.wait_with_timeout(timeout_ns)) {
								*frame->ret() = ERR_TRY_AGAIN;
								break;
							}
						}
					}
				}
			}

			break;
		}
		case SYS_SHUTDOWN:
		{
			auto type = static_cast<ShutdownType>(*frame->arg0());
#ifdef __x86_64__
			switch (type) {
				case SHUTDOWN_TYPE_POWER_OFF:
				{
					acpi::enter_sleep_state(acpi::SleepState::S5);
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				}
				case SHUTDOWN_TYPE_REBOOT:
					acpi::reboot();
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case SHUTDOWN_TYPE_SLEEP:
					acpi::enter_sleep_state(acpi::SleepState::S3);
					*frame->ret() = 0;
					break;
				default:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
			}
#elif defined(__aarch64__)
			switch (type) {
				case SHUTDOWN_TYPE_POWER_OFF:
				{
					psci_system_off();
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				}
				case SHUTDOWN_TYPE_REBOOT:
					psci_system_reset();
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case SHUTDOWN_TYPE_SLEEP:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
			}
#else
#error Shutdown is not implemented
#endif
			break;
		}
		case SYS_SERVICE_CREATE:
		{
			if (thread->process->service) {
				*frame->ret() = ERR_ALREADY_EXISTS;
				break;
			}

			usize feature_count = *frame->arg1();
			kstd::vector<kstd::string> features;
			features.resize(feature_count);
			bool success = true;
			for (usize i = 0; i < feature_count; ++i) {
				CrescentStringView view;
				if (!UserAccessor(*frame->arg0() + i * sizeof(CrescentStringView)).load(view)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
				features[i].resize_without_null(view.len);
				if (!UserAccessor(view.str).load(features[i].data(), view.len)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
			}

			if (!success) {
				break;
			}

			println("[kernel]: process ", thread->process->name, " advertises features: ");
			for (auto& feature : features) {
				println("\t", feature);
			}

			service_create(std::move(features), thread->process);
			*frame->ret() = 0;

			break;
		}
		case SYS_SERVICE_GET:
		{
			usize feature_count = *frame->arg2();
			kstd::vector<kstd::string> needed_features;
			needed_features.resize(feature_count);
			bool success = true;
			for (usize i = 0; i < feature_count; ++i) {
				CrescentStringView view;
				if (!UserAccessor(*frame->arg1() + i * sizeof(CrescentStringView)).load(view)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
				needed_features[i].resize_without_null(view.len);
				if (!UserAccessor(view.str).load(needed_features[i].data(), view.len)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
			}

			if (!success) {
				break;
			}

			auto service = service_get(needed_features);
			if (!service) {
				*frame->ret() = ERR_NOT_EXISTS;
				break;
			}
			auto handle = thread->process->handles.insert(std::move(service));

			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_SOCKET_CREATE:
		{
			auto type = static_cast<SocketType>(*frame->arg1());
			int flags = static_cast<int>(*frame->arg2());

			switch (type) {
				case SOCKET_TYPE_IPC:
				{
					IrqGuard irq_guard {};
					auto ipc_guard = thread->process->ipc_socket.lock();
					if (!*ipc_guard) {
						auto desc = kstd::make_shared<ProcessDescriptor>(thread->process, 0);
						thread->process->add_descriptor(desc.data());

						*ipc_guard = kstd::make_shared<IpcSocket>(std::move(desc), flags);
					}
					kstd::shared_ptr<Socket> copy {*ipc_guard};
					auto handle = thread->process->handles.insert(std::move(copy));

					if (!UserAccessor(*frame->arg0()).store(handle)) {
						thread->process->handles.remove(handle);
						*frame->ret() = ERR_FAULT;
						break;
					}

					*frame->ret() = 0;
					break;
				}
				case SOCKET_TYPE_UDP:
				{
					auto socket = udp_socket_create(flags);
					auto handle = thread->process->handles.insert(std::move(socket));

					if (!UserAccessor(*frame->arg0()).store(handle)) {
						thread->process->handles.remove(handle);
						*frame->ret() = ERR_FAULT;
						break;
					}

					*frame->ret() = 0;
					break;
				}
				case SOCKET_TYPE_TCP:
				{
					auto socket = tcp_socket_create(flags);
					auto handle = thread->process->handles.insert(std::move(socket));

					if (!UserAccessor(*frame->arg0()).store(handle)) {
						thread->process->handles.remove(handle);
						*frame->ret() = ERR_FAULT;
						break;
					}

					*frame->ret() = 0;
					break;
				}
			}

			break;
		}
		case SYS_SOCKET_CONNECT:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			SocketAddress generic_addr;
			if (!UserAccessor(*frame->arg1()).load(generic_addr)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto socket = socket_ptr->data();

			AnySocketAddress addr {};
			if (generic_addr.type == SOCKET_ADDRESS_TYPE_IPC) {
				IpcSocketAddress ipc;
				if (!UserAccessor(*frame->arg1()).load(ipc)) {
					*frame->ret() = ERR_FAULT;
					break;
				}

				auto target = thread->process->handles.get(ipc.target);
				kstd::shared_ptr<ProcessDescriptor>* target_desc;
				if (!target || !(target_desc = target->get<kstd::shared_ptr<ProcessDescriptor>>())) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				addr.ipc.generic.type = SOCKET_ADDRESS_TYPE_IPC;
				addr.ipc.descriptor = target_desc->data();
			}
			else {
				if (!UserAccessor(*frame->arg1()).load(&addr, socket_address_type_to_size(generic_addr.type))) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			*frame->ret() = socket->connect(addr);

			break;
		}
		case SYS_SOCKET_LISTEN:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto port = static_cast<uint32_t>(*frame->arg1());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto socket = socket_ptr->data();
			*frame->ret() = socket->listen(port);

			break;
		}
		case SYS_SOCKET_ACCEPT:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			int connection_flags = static_cast<int>(*frame->arg2());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto socket = socket_ptr->data();
			kstd::shared_ptr<Socket> connection {nullptr};
			auto ret = socket->accept(connection, connection_flags);

			if (ret == 0) {
				auto connection_handle = thread->process->handles.insert(std::move(connection));

				if (!UserAccessor(*frame->arg1()).store(connection_handle)) {
					thread->process->handles.remove(connection_handle);
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			*frame->ret() = ret;

			break;
		}
		case SYS_SOCKET_SEND:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto size = *frame->arg2();

			kstd::vector<u8> data;
			data.resize(size);
			if (!UserAccessor(*frame->arg1()).load(data.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto socket = socket_ptr->data();
			*frame->ret() = socket->send(data.data(), size);

			if (!UserAccessor(*frame->arg3()).store(size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			break;
		}
		case SYS_SOCKET_SEND_TO:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto size = *frame->arg2();

			kstd::vector<u8> data;
			data.resize(size);
			if (!UserAccessor(*frame->arg1()).load(data.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			SocketAddress generic_addr;
			if (!UserAccessor(*frame->arg3()).load(generic_addr)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			AnySocketAddress addr {};
			if (generic_addr.type == SOCKET_ADDRESS_TYPE_IPC) {
				IpcSocketAddress ipc;
				if (!UserAccessor(*frame->arg3()).load(ipc)) {
					*frame->ret() = ERR_FAULT;
					break;
				}

				auto target = thread->process->handles.get(ipc.target);
				kstd::shared_ptr<ProcessDescriptor>* target_desc;
				if (!target || !(target_desc = target->get<kstd::shared_ptr<ProcessDescriptor>>())) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				addr.ipc.generic.type = SOCKET_ADDRESS_TYPE_IPC;
				addr.ipc.descriptor = target_desc->data();
			}
			else {
				if (!UserAccessor(*frame->arg3()).load(&addr, socket_address_type_to_size(generic_addr.type))) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			auto socket = socket_ptr->data();
			*frame->ret() = socket->send_to(data.data(), size, addr);

			break;
		}
		case SYS_SOCKET_RECEIVE:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			usize size = *frame->arg2();

			auto socket = socket_ptr->data();

			kstd::vector<u8> data;
			data.resize(size);
			*frame->ret() = socket->receive(data.data(), size);

			if (!UserAccessor(*frame->arg1()).store(data.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			if (!UserAccessor(*frame->arg3()).store(size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			break;
		}
		case SYS_SOCKET_RECEIVE_FROM:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			usize size = *frame->arg2();

			auto socket = socket_ptr->data();

			AnySocketAddress addr {};

			kstd::vector<u8> data;
			data.resize(size);
			auto ret = socket->receive_from(data.data(), size, addr);
			*frame->ret() = ret;

			if (ret == 0) {
				if (!UserAccessor(*frame->arg1()).store(data.data(), size)) {
					*frame->ret() = ERR_FAULT;
					break;
				}

				if (!UserAccessor(*frame->arg3()).store(size)) {
					*frame->ret() = ERR_FAULT;
					break;
				}

				if (addr.generic.type == SOCKET_ADDRESS_TYPE_IPC) {
					IpcSocketAddress ipc_addr {
						.generic {
							.type = SOCKET_ADDRESS_TYPE_IPC
						},
						.target = INVALID_CRESCENT_HANDLE
					};

					if (!UserAccessor(*frame->arg4()).store(ipc_addr)) {
						*frame->ret() = ERR_FAULT;
						break;
					}
				}
				else {
					if (!UserAccessor(*frame->arg4()).store(&addr, socket_address_type_to_size(addr.generic.type))) {
						*frame->ret() = ERR_FAULT;
						break;
					}
				}
			}

			break;
		}
		case SYS_SOCKET_GET_PEER_NAME:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<Socket>* socket_ptr;
			if (!handle || !(socket_ptr = handle->get<kstd::shared_ptr<Socket>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			AnySocketAddress peer_address {};
			if (auto status = (*socket_ptr)->get_peer_name(peer_address); status != 0) {
				*frame->ret() = status;
				break;
			}

			if (peer_address.generic.type == SOCKET_ADDRESS_TYPE_IPC) {
				auto proc_desc = peer_address.ipc.descriptor->duplicate();
				auto proc_handle = thread->process->handles.insert(std::move(proc_desc));

				IpcSocketAddress addr {
					.generic {
						.type = SOCKET_ADDRESS_TYPE_IPC
					},
					.target = proc_handle
				};

				if (!UserAccessor(*frame->arg1()).store(addr)) {
					thread->process->handles.remove(proc_handle);
					*frame->ret() = ERR_FAULT;
					break;
				}
			}
			else if (peer_address.generic.type == SOCKET_ADDRESS_TYPE_IPV4) {
				if (!UserAccessor(*frame->arg1()).store(peer_address.ipv4)) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}
			else if (peer_address.generic.type == SOCKET_ADDRESS_TYPE_IPV6) {
				if (!UserAccessor(*frame->arg1()).store(peer_address.ipv6)) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_SHARED_MEM_ALLOC:
		{
			usize size = *frame->arg1();
			auto mem = kstd::make_shared<SharedMemory>();
			bool success = true;

			for (usize i = 0; i < size; i += PAGE_SIZE) {
				auto page = pmalloc(1);
				if (!page) {
					success = false;
					*frame->ret() = ERR_NO_MEM;
					break;
				}
				mem->pages.push(page);
			}

			if (!success) {
				break;
			}

			auto handle = thread->process->handles.insert(std::move(mem));
			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_SHARED_MEM_MAP:
		{
			CrescentHandle user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<SharedMemory>* shared_mem_ptr;
			if (!handle || !(shared_mem_ptr = handle->get<kstd::shared_ptr<SharedMemory>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			SharedMemory* shared_mem = shared_mem_ptr->data();

			auto mem = thread->process->allocate(
				nullptr,
				shared_mem->pages.size() * PAGE_SIZE,
				PageFlags::Read | PageFlags::Write,
				MemoryAllocFlags::None,
				nullptr);
			if (!mem) {
				*frame->ret() = ERR_NO_MEM;
				break;
			}

			bool success = true;

			for (usize i = 0; i < shared_mem->pages.size() * PAGE_SIZE; i += PAGE_SIZE) {
				bool ret = thread->process->page_map.map(
					mem + i,
					shared_mem->pages[i / PAGE_SIZE],
					PageFlags::Read | PageFlags::Write | PageFlags::User,
					CacheMode::WriteBack);
				if (!ret) {
					for (usize j = 0; j < i; j += PAGE_SIZE) {
						thread->process->page_map.unmap(mem + j);
					}

					success = false;
					thread->process->free(mem, shared_mem->pages.size() * PAGE_SIZE);
					*frame->ret() = ERR_NO_MEM;
					break;
				}
			}

			if (!success) {
				break;
			}

			if (!UserAccessor(*frame->arg1()).store(reinterpret_cast<void*>(mem))) {
				for (usize i = 0; i < shared_mem->pages.size() * PAGE_SIZE; i += PAGE_SIZE) {
					thread->process->page_map.unmap(mem + i);
				}
				thread->process->free(mem, shared_mem->pages.size() * PAGE_SIZE);

				*frame->ret() = ERR_FAULT;
				break;
			}

			auto guard = shared_mem->usage_count.lock();
			++*guard;

			*frame->ret() = 0;

			break;
		}
		case SYS_SHARED_MEM_SHARE:
		{
			CrescentHandle user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<SharedMemory>* shared_mem_ptr;
			if (!handle || !(shared_mem_ptr = handle->get<kstd::shared_ptr<SharedMemory>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			CrescentHandle user_proc_handle = *frame->arg1();
			auto proc_handle = thread->process->handles.get(user_proc_handle);
			if (!proc_handle) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			Spinlock<Process*>* process_lock;
			if (auto proc_desc_ptr = proc_handle->get<kstd::shared_ptr<ProcessDescriptor>>()) {
				process_lock = &proc_desc_ptr->data()->process;
			}
			else {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto copy = *shared_mem_ptr;
			auto guard = process_lock->lock();
			if (!*guard) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto res_handle = (*guard)->handles.insert(std::move(copy));

			if (!UserAccessor(*frame->arg2()).store(res_handle)) {
				(*guard)->handles.remove(res_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_OPENAT:
		{
			kstd::string path;
			path.resize_without_null(*frame->arg3());
			if (!UserAccessor(*frame->arg2()).load(path.data(), *frame->arg3())) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			CrescentHandle dir_handle = *frame->arg1();

			kstd::shared_ptr<VNode> start;
			if (dir_handle != INVALID_CRESCENT_HANDLE) {
				auto dir_entry = thread->process->handles.get(dir_handle);
				kstd::shared_ptr<OpenFile>* ptr;
				if (!dir_entry || !(ptr = dir_entry->get<kstd::shared_ptr<OpenFile>>())) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}

				start = (*ptr)->node;
			}
			else {
				start = INITRD_VFS->get_root();
			}

			int flags = static_cast<int>(*frame->arg4());

			auto node = vfs_lookup(std::move(start), path);
			if (!node) {
				*frame->ret() = ERR_NOT_EXISTS;
				break;
			}

			auto new_file = kstd::make_shared<OpenFile>(std::move(node));

			auto handle = thread->process->handles.insert(std::move(new_file));
			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_READ:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto size = static_cast<size_t>(*frame->arg2());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto& file = *file_ptr;

			kstd::vector<u8> buffer;
			buffer.resize(size);

			auto status = file->node->read(buffer.data(), size, file->cursor);
			switch (status) {
				case FsStatus::Success:
					if (file->node->seekable) {
						file->cursor += size;
					}

					if (!UserAccessor(*frame->arg1()).store(buffer.data(), size)) {
						*frame->ret() = ERR_FAULT;
					}
					else {
						*frame->ret() = 0;
					}

					if (*frame->arg3()) {
						if (!UserAccessor(*frame->arg3()).store(size)) {
							*frame->ret() = ERR_FAULT;
						}
					}

					break;
				case FsStatus::Unsupported:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case FsStatus::OutOfBounds:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				case FsStatus::TryAgain:
					*frame->ret() = ERR_TRY_AGAIN;
					break;
			}

			break;
		}
		case SYS_WRITE:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto size = static_cast<size_t>(*frame->arg2());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto& file = *file_ptr;

			kstd::vector<u8> buffer;
			buffer.resize(size);
			if (!UserAccessor(*frame->arg1()).load(buffer.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto status = file->node->write(buffer.data(), size, file->cursor);
			switch (status) {
				case FsStatus::Success:
					if (file->node->seekable) {
						file->cursor += size;
					}

					*frame->ret() = 0;

					if (*frame->arg3()) {
						if (!UserAccessor(*frame->arg3()).store(size)) {
							*frame->ret() = ERR_FAULT;
						}
					}

					break;
				case FsStatus::Unsupported:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case FsStatus::OutOfBounds:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				case FsStatus::TryAgain:
					*frame->ret() = ERR_TRY_AGAIN;
					break;
			}

			break;

		}
		case SYS_SEEK:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto offset = static_cast<int64_t>(*frame->arg1());
			int whence = static_cast<int>(*frame->arg2());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto& file = *file_ptr;

			if (!file->node->seekable) {
				*frame->ret() = ERR_UNSUPPORTED;
				break;
			}

			if (whence == SEEK_START) {
				file->cursor = offset;
			}
			else if (whence == SEEK_CURRENT) {
				file->cursor += offset;
			}
			else if (whence == SEEK_END) {
				FsStat stat {};
				auto status = file->node->stat(stat);
				switch (status) {
					case FsStatus::Success:
						*frame->ret() = 0;
						break;
					case FsStatus::Unsupported:
						*frame->ret() = ERR_UNSUPPORTED;
						break;
					case FsStatus::OutOfBounds:
						*frame->ret() = ERR_INVALID_ARGUMENT;
						break;
					case FsStatus::TryAgain:
						*frame->ret() = ERR_TRY_AGAIN;
						break;
				}

				if (status != FsStatus::Success) {
					break;
				}

				file->cursor = stat.size + offset;
				break;
			}
			else {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			if (*frame->arg3()) {
				if (!UserAccessor(*frame->arg3()).store(file->cursor)) {
					*frame->ret() = ERR_FAULT;
					break;
				}
			}

			*frame->ret() = 0;
			break;
		}
		case SYS_STAT:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto& file = *file_ptr;

			FsStat stat {};
			auto status = file->node->stat(stat);
			switch (status) {
				case FsStatus::Success:
				{
					CrescentStat user_stat {
						.size = stat.size
					};

					if (!UserAccessor(*frame->arg1()).store(user_stat)) {
						*frame->ret() = ERR_FAULT;
					}
					else {
						*frame->ret() = 0;
					}

					break;
				}
				case FsStatus::Unsupported:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case FsStatus::OutOfBounds:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				case FsStatus::TryAgain:
					*frame->ret() = ERR_TRY_AGAIN;
					break;
			}

			break;
		}
		case SYS_LIST_DIR:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<OpenFile>* file_ptr;
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto& file = *file_ptr;

			usize max_count;
			if (!UserAccessor(*frame->arg2()).load(max_count)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			// todo don't expose this to userspace
			usize offset;
			if (!UserAccessor(*frame->arg3()).load(offset)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			if (max_count > 1000) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			kstd::vector<DirEntry> entries;
			entries.resize(max_count);
			auto status = file->node->list_dir(entries.data(), max_count, offset);
			switch (status) {
				case FsStatus::Success:
				{
					if (!UserAccessor(*frame->arg2()).store(max_count)) {
						*frame->ret() = ERR_FAULT;
						break;
					}

					if (*frame->arg1()) {
						if (!UserAccessor(*frame->arg1()).store(entries.data(), max_count * sizeof(DirEntry))) {
							*frame->ret() = ERR_FAULT;
							break;
						}

						if (!UserAccessor(*frame->arg3()).store(offset)) {
							*frame->ret() = ERR_FAULT;
							break;
						}
					}

					*frame->ret() = 0;

					break;
				}
				case FsStatus::Unsupported:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case FsStatus::OutOfBounds:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				case FsStatus::TryAgain:
					*frame->ret() = ERR_TRY_AGAIN;
					break;
			}

			break;
		}
		case SYS_PIPE_CREATE:
		{
			auto max_size = static_cast<size_t>(*frame->arg2());
			int read_flags = static_cast<int>(*frame->arg3());
			int write_flags = static_cast<int>(*frame->arg4());

			auto opt = PipeVNode::create(max_size, static_cast<FileFlags>(read_flags), static_cast<FileFlags>(write_flags));
			if (!opt) {
				*frame->ret() = ERR_NO_MEM;
				break;
			}

			auto [reading, writing] = std::move(opt).value();
			auto reading_ptr {kstd::make_shared<PipeVNode>(std::move(reading))};
			auto writing_ptr {kstd::make_shared<PipeVNode>(std::move(writing))};
			auto reading_file = kstd::make_shared<OpenFile>(std::move(reading_ptr));
			auto writing_file = kstd::make_shared<OpenFile>(std::move(writing_ptr));

			auto reading_handle = thread->process->handles.insert(std::move(reading_file));
			auto writing_handle = thread->process->handles.insert(std::move(writing_file));

			if (!UserAccessor(*frame->arg0()).store(reading_handle)) {
				thread->process->handles.remove(reading_handle);
				thread->process->handles.remove(writing_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			if (!UserAccessor(*frame->arg1()).store(writing_handle)) {
				thread->process->handles.remove(reading_handle);
				thread->process->handles.remove(writing_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

			break;
		}
		case SYS_REPLACE_STD_HANDLE:
		{
			auto user_std_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto new_handle = static_cast<CrescentHandle>(*frame->arg1());

			if (user_std_handle != STDIN_HANDLE && user_std_handle != STDOUT_HANDLE && user_std_handle != STDERR_HANDLE) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			kstd::shared_ptr<OpenFile>* file_ptr;
			auto handle = thread->process->handles.get(new_handle);
			if (!handle || !(file_ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}
			auto file = *file_ptr;

			thread->process->handles.replace(user_std_handle, std::move(file));

			*frame->ret() = 0;

			break;
		}
		case SYS_FUTEX_WAIT:
		{
			auto* ptr = reinterpret_cast<kstd::atomic<int>*>(*frame->arg0());
			auto value = static_cast<int>(*frame->arg1());
			auto timeout_ns = *frame->arg2();

			IrqGuard irq_guard {};

			bool state;
			{
				auto guard = thread->process->futexes.lock();

				// todo if this faults the process is killed
				auto current_value = ptr->load(kstd::memory_order::seq_cst);
				if (current_value != value) {
					*frame->ret() = ERR_TRY_AGAIN;
					break;
				}

				auto entry = guard->find<kstd::atomic<int>*, &Process::Futex::ptr>(ptr);
				if (!entry) {
					auto* new_entry = new Process::Futex {
						.ptr = ptr
					};
					guard->insert(new_entry);
					entry = new_entry;
				}

				entry->waiters.push(thread);
				thread->sleep_interrupted = false;
				if (timeout_ns == UINT64_MAX) {
					state = thread->cpu->scheduler.prepare_for_block();
				}
				else {
					if (timeout_ns > SCHED_MAX_SLEEP_US * NS_IN_US) {
						timeout_ns = SCHED_MAX_SLEEP_US * NS_IN_US;
					}
					state = thread->cpu->scheduler.prepare_for_sleep(timeout_ns);
				}
			}

			if (timeout_ns == UINT64_MAX) {
				thread->cpu->scheduler.block(state);
			}
			else {
				thread->cpu->scheduler.sleep(state);

				if (thread->sleep_interrupted) {
					*frame->ret() = ERR_TIMEOUT;
				}
				else {
					*frame->ret() = 0;
				}
			}

			break;
		}
		case SYS_FUTEX_WAKE:
		{
			auto* ptr = reinterpret_cast<kstd::atomic<int>*>(*frame->arg0());
			auto count = static_cast<int>(*frame->arg1());

			IrqGuard irq_guard {};
			auto guard = thread->process->futexes.lock();

			auto entry = guard->find<kstd::atomic<int>*, &Process::Futex::ptr>(ptr);
			if (!entry) {
				*frame->ret() = 0;
				break;
			}

			assert(!entry->waiters.is_empty());
			for (; !entry->waiters.is_empty() && count > 0; --count) {
				auto waiter = entry->waiters.pop_front();
				auto thread_guard = waiter->sched_lock.lock();
				assert(thread->status == Thread::Status::Sleeping);
				thread->cpu->scheduler.unblock(thread, true, true);
			}

			if (entry->waiters.is_empty()) {
				guard->remove(entry);
				delete entry;
			}

			break;
		}
		case SYS_SET_FS_BASE:
		{
#ifdef __x86_64__
			thread->fs_base = *frame->arg0();
			msrs::IA32_FSBASE.write(thread->fs_base);
			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_SET_GS_BASE:
		{
#ifdef __x86_64__
			thread->gs_base = *frame->arg0();
			msrs::IA32_KERNEL_GSBASE.write(thread->gs_base);
			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_GET_FS_BASE:
		{
#ifdef __x86_64__
			if (!UserAccessor(*frame->arg0()).store(thread->fs_base)) {
				*frame->ret() = ERR_FAULT;
			}
			else {
				*frame->ret() = 0;
			}
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_GET_GS_BASE:
		{
#ifdef __x86_64__
			if (!UserAccessor(*frame->arg0()).store(thread->gs_base)) {
				*frame->ret() = ERR_FAULT;
			}
			else {
				*frame->ret() = 0;
			}
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_GET_ARCH_INFO:
		{
#ifdef __x86_64__
			IrqGuard irq_guard {};
			X86ArchInfo info {
				.tsc_frequency = get_current_thread()->cpu->tsc_freq
			};
			if (!UserAccessor(*frame->arg0()).store(info)) {
				*frame->ret() = ERR_FAULT;
				break;
			}
			else {
				*frame->ret() = 0;
			}
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_CREATE:
		{
#ifdef __x86_64__
			if (!vmx_supported()) {
				*frame->ret() = ERR_UNSUPPORTED;
				break;
			}

			auto vm = vmx_create_vm();
			auto handle = thread->process->handles.insert(std::move(vm));
			if (!UserAccessor(*frame->arg0()).store(handle)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_CREATE_VCPU:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::Evm>* evm_ptr;
			if (!handle || !(evm_ptr = handle->get<kstd::shared_ptr<evm::Evm>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& evm = *evm_ptr->data();

			auto status = evm.create_vcpu();
			if (!status) {
				*frame->ret() = status.error();
				break;
			}

			auto vcpu = std::move(status).value();
			auto vcpu_handle = thread->process->handles.insert(vcpu);
			if (!UserAccessor(*frame->arg1()).store(vcpu_handle)) {
				thread->process->handles.remove(vcpu_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto state_addr = thread->process->allocate(
				nullptr,
				PAGE_SIZE,
				PageFlags::Read | PageFlags::Write,
				MemoryAllocFlags::None,
				nullptr);
			if (!state_addr) {
				thread->process->handles.remove(vcpu_handle);
				*frame->ret() = ERR_NO_MEM;
				break;
			}

			auto map_status = thread->process->page_map.map(
				state_addr,
				vcpu->state_phys,
				PageFlags::Read | PageFlags::Write | PageFlags::User,
				CacheMode::WriteBack);
			if (!map_status) {
				thread->process->free(state_addr, PAGE_SIZE);
				thread->process->handles.remove(vcpu_handle);
				*frame->ret() = ERR_NO_MEM;
				break;
			}

			if (!UserAccessor(*frame->arg2()).store(state_addr)) {
				thread->process->page_map.unmap(state_addr);
				thread->process->free(state_addr, PAGE_SIZE);
				thread->process->handles.remove(vcpu_handle);
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_MAP:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::Evm>* evm_ptr;
			if (!handle || !(evm_ptr = handle->get<kstd::shared_ptr<evm::Evm>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& evm = *evm_ptr->data();

			auto guest = *frame->arg1();
			auto host_virt = *frame->arg2();
			auto size = *frame->arg3();

			if (host_virt & (PAGE_SIZE - 1)) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto guard = thread->process->mappings.lock();

			bool success = true;
			for (usize i = 0; i < size; i += PAGE_SIZE) {
				auto phys = thread->process->page_map.get_phys(host_virt + i);
				if (!phys) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					success = false;
					break;
				}

				auto page = Page::from_phys(phys);
				assert(page);
				auto status = evm.map_page(guest + i, page->phys());
				if (status != 0) {
					for (usize j = 0; j < i; j += PAGE_SIZE) {
						evm.unmap_page(guest + j);
					}

					*frame->ret() = status;
					success = false;
					break;
				}
			}

			if (!success) {
				break;
			}

			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_UNMAP:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::Evm>* evm_ptr;
			if (!handle || !(evm_ptr = handle->get<kstd::shared_ptr<evm::Evm>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& evm = *evm_ptr->data();

			auto guest = *frame->arg1();
			auto size = *frame->arg3();

			if (guest & (PAGE_SIZE - 1)) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto guard = thread->process->mappings.lock();

			for (usize i = 0; i < size; i += PAGE_SIZE) {
				evm.unmap_page(guest + i);
			}

			*frame->ret() = 0;
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_VCPU_RUN:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::VirtualCpu>* vcpu_ptr;
			if (!handle || !(vcpu_ptr = handle->get<kstd::shared_ptr<evm::VirtualCpu>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& vcpu = *vcpu_ptr->data();

			*frame->ret() = vcpu.run();
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_VCPU_WRITE_STATE:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::VirtualCpu>* vcpu_ptr;
			if (!handle || !(vcpu_ptr = handle->get<kstd::shared_ptr<evm::VirtualCpu>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& vcpu = *vcpu_ptr->data();

			*frame->ret() = vcpu.write_state(static_cast<EvmStateBits>(*frame->arg1()));
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_VCPU_READ_STATE:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::VirtualCpu>* vcpu_ptr;
			if (!handle || !(vcpu_ptr = handle->get<kstd::shared_ptr<evm::VirtualCpu>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& vcpu = *vcpu_ptr->data();

			*frame->ret() = vcpu.read_state(static_cast<EvmStateBits>(*frame->arg1()));
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		case SYS_EVM_VCPU_TRIGGER_IRQ:
		{
#ifdef __x86_64__
			auto user_handle = *frame->arg0();
			auto handle = thread->process->handles.get(user_handle);

			kstd::shared_ptr<evm::VirtualCpu>* vcpu_ptr;
			if (!handle || !(vcpu_ptr = handle->get<kstd::shared_ptr<evm::VirtualCpu>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			auto& vcpu = *vcpu_ptr->data();

			EvmIrqInfo info {};
			if (!UserAccessor(*frame->arg1()).load(info)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = vcpu.trigger_irq(info);
#else
			*frame->ret() = ERR_UNSUPPORTED;
#endif
			break;
		}
		default:
			println("[kernel]: invalid syscall ", num);
			*frame->ret() = ERR_INVALID_ARGUMENT;
			break;
	}
}
