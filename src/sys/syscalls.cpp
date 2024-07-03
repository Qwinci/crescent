#include "syscalls.hpp"
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

extern void hda_play();

#ifdef __x86_64__
#include "acpi/sleep.hpp"
#elif defined(__aarch64__)
#include "arch/aarch64/dev/psci.hpp"
#include "exe/elf_loader.hpp"
#include "fs/vfs.hpp"
#endif

extern "C" bool mem_copy_to_user(usize user, const void* kernel, usize size);
extern "C" bool mem_copy_to_kernel(void* kernel, usize user, usize size);

struct UserAccessor {
	constexpr explicit UserAccessor(usize addr) : addr {addr} {}
	template<typename T>
	inline explicit UserAccessor(T* ptr) : addr {reinterpret_cast<usize>(ptr)} {}

	bool load(void* data, usize size) {
		return mem_copy_to_kernel(data, addr, size);
	}

	template<typename T>
	bool load(T& data) {
		return load(static_cast<void*>(&data), sizeof(T));
	}

	bool store(const void* data, usize size) {
		return mem_copy_to_user(addr, data, size);
	}

	template<typename T>
	bool store(const T& data) {
		return store(static_cast<const void*>(&data), sizeof(T));
	}

private:
	usize addr;
};

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

			auto node = INITRD_VFS->lookup(path_view);
			if (!node) {
				*frame->ret() = ERR_NOT_EXISTS;
				break;
			}

			Process* process;
			if (process_info.flags & PROCESS_STD_HANDLES) {
				auto stdin = thread->process->handles.get(process_info.stdin_handle);
				if (!stdin || !stdin->get<kstd::shared_ptr<VNode>>()) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				auto stdout = thread->process->handles.get(process_info.stdout_handle);
				if (!stdout || !stdout->get<kstd::shared_ptr<VNode>>()) {
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
				}
				auto stderr = thread->process->handles.get(process_info.stderr_handle);
				if (!stderr || !stderr->get<kstd::shared_ptr<VNode>>()) {
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
				elf_result.value().entry,
				nullptr
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
		case SYS_SLEEP:
		{
			auto us = *frame->arg0();
			if (us > SCHED_MAX_SLEEP_US) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			thread->sleep_for(us);
			*frame->ret() = 0;
			break;
		}
		case SYS_GET_TIME:
		{
			u64 now;
			{
				IrqGuard irq_guard {};
				auto guard = CLOCK_SOURCE.lock_read();
				now = (*guard)->get() / (*guard)->ticks_in_us;
			}

			if (!UserAccessor(*frame->arg0()).store(now)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			*frame->ret() = 0;

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
			println(str);

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
			if (protection & CRESCENT_PROT_READ) {
				flags |= MemoryAllocFlags::Read;
			}
			if (protection & CRESCENT_PROT_WRITE) {
				flags |= MemoryAllocFlags::Write;
			}
			if (protection & CRESCENT_PROT_EXEC) {
				flags |= MemoryAllocFlags::Execute;
			}

			auto addr = thread->process->allocate(ptr, size, flags, nullptr);
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

					auto guard = DEVICES[static_cast<int>(type)]->lock();
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

					auto guard = DEVICES[static_cast<int>(req.data.open_device.type)]->lock();
					for (auto& device : *guard) {
						if (device->name == name) {
							auto handle = thread->process->handles.insert(device);
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
					kstd::shared_ptr<Device>* device;
					if (!handle || !(device = handle->get<kstd::shared_ptr<Device>>())) {
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
					auto status = (*device)->handle_request(req_data, resp_data, link.response_buf_size - sizeof(DevLinkResponse));
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
		case SYS_POLL_EVENT:
		{
			InputEvent event {};
			size_t timeout_us = *frame->arg1();
			if (timeout_us != SIZE_MAX && timeout_us > SCHED_MAX_SLEEP_US) {
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
					if (!timeout_us) {
						*frame->ret() = ERR_TRY_AGAIN;
						break;
					}
					else {
						if (timeout_us == SIZE_MAX) {
							GLOBAL_EVENT_QUEUE.produce_event.wait();
						}
						else {
							if (!GLOBAL_EVENT_QUEUE.produce_event.wait_with_timeout(timeout_us)) {
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
				MemoryAllocFlags::Read | MemoryAllocFlags::Write,
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
		case SYS_OPEN:
		{
			kstd::string path;
			path.resize_without_null(*frame->arg2());
			if (!UserAccessor(*frame->arg1()).load(path.data(), *frame->arg2())) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			int flags = static_cast<int>(*frame->arg3());

			auto node = INITRD_VFS->lookup(path);
			if (!node) {
				*frame->ret() = ERR_NOT_EXISTS;
				break;
			}

			auto handle = thread->process->handles.insert(std::move(node));
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
			auto offset = static_cast<size_t>(*frame->arg2());
			auto size = static_cast<size_t>(*frame->arg3());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<VNode>* vnode;
			if (!handle || !(vnode = handle->get<kstd::shared_ptr<VNode>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			kstd::vector<u8> buffer;
			buffer.resize(size);

			auto status = (*vnode)->read(buffer.data(), size, offset);
			switch (status) {
				case FsStatus::Success:
					if (!UserAccessor(*frame->arg1()).store(buffer.data(), buffer.size())) {
						*frame->ret() = ERR_FAULT;
					}
					else {
						*frame->ret() = 0;
					}

					break;
				case FsStatus::Unsupported:
					*frame->ret() = ERR_UNSUPPORTED;
					break;
				case FsStatus::OutOfBounds:
					*frame->ret() = ERR_INVALID_ARGUMENT;
					break;
			}

			break;
		}
		case SYS_WRITE:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());
			auto offset = static_cast<size_t>(*frame->arg2());
			auto size = static_cast<size_t>(*frame->arg3());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<VNode>* vnode;
			if (!handle || !(vnode = handle->get<kstd::shared_ptr<VNode>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			kstd::vector<u8> buffer;
			buffer.resize(size);
			if (!UserAccessor(*frame->arg1()).load(buffer.data(), size)) {
				*frame->ret() = ERR_FAULT;
				break;
			}

			auto status = (*vnode)->write(buffer.data(), size, offset);
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
			}

			break;
		}
		case SYS_STAT:
		{
			auto user_handle = static_cast<CrescentHandle>(*frame->arg0());

			auto handle = thread->process->handles.get(user_handle);
			kstd::shared_ptr<VNode>* vnode;
			if (!handle || !(vnode = handle->get<kstd::shared_ptr<VNode>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			FsStat stat {};
			auto status = (*vnode)->stat(stat);
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
			}

			break;
		}
		case SYS_PIPE_CREATE:
		{
			auto max_size = static_cast<size_t>(*frame->arg2());

			auto opt = PipeVNode::create(max_size);
			if (!opt) {
				*frame->ret() = ERR_NO_MEM;
				break;
			}

			auto [reading, writing] = std::move(opt).value();
			kstd::shared_ptr<VNode> reading_ptr {kstd::make_shared<PipeVNode>(std::move(reading))};
			kstd::shared_ptr<VNode> writing_ptr {kstd::make_shared<PipeVNode>(std::move(writing))};

			auto reading_handle = thread->process->handles.insert(std::move(reading_ptr));
			auto writing_handle = thread->process->handles.insert(std::move(writing_ptr));

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

			kstd::shared_ptr<VNode>* vnode;
			auto handle = thread->process->handles.get(new_handle);
			if (!handle || !(vnode = handle->get<kstd::shared_ptr<VNode>>())) {
				*frame->ret() = ERR_INVALID_ARGUMENT;
				break;
			}

			thread->process->handles.replace(user_std_handle, std::move(*vnode));

			*frame->ret() = 0;

			break;
		}
		default:
			println("[kernel]: invalid syscall ", num);
			*frame->ret() = ERR_INVALID_ARGUMENT;
			break;
	}
}
