#include "syscalls.hpp"
#include "arch/cpu.hpp"
#include "crescent/devlink.h"
#include "crescent/syscalls.h"
#include "event_queue.hpp"
#include "sched/process.hpp"
#include "sched/sched.hpp"
#include "stdio.hpp"
#include "fs/vfs.hpp"
#include "exe/elf_loader.hpp"

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

			CrescentHandle handle = thread->process->handles.insert(ThreadDescriptor {
				.thread {new_thread},
				.exit_status = 0
			});
			auto descriptor = thread->process->handles.get(handle)->get<ThreadDescriptor>();
			new_thread->add_descriptor(descriptor);

			if (!UserAccessor(*frame->arg0()).store(handle)) {
				thread->process->handles.remove(handle);
				delete new_thread;
				*frame->ret() = ERR_FAULT;
				break;
			}

			println("[kernel]: create thread ", name);

			IrqGuard guard {};
			cpu->scheduler.queue(new_thread);

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
			usize arg_count = *frame->arg4();
			kstd::vector<kstd::string> args;
			args.resize(arg_count);
			bool success = true;
			for (usize i = 0; i < arg_count; ++i) {
				CrescentStringView view;
				if (!UserAccessor(*frame->arg3() + i * sizeof(CrescentStringView)).load(view)) {
					*frame->ret() = ERR_FAULT;
					success = false;
					break;
				}
				args[i].resize_without_null(view.len);
				if (!UserAccessor(view.str).load(view.len)) {
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

			auto* process = new Process {name, true};
			auto elf_result = elf_load(process, node);
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

			CrescentHandle handle = thread->process->handles.insert(ProcessDescriptor {
				.process {process},
				.exit_status = 0
			});
			auto descriptor = thread->process->handles.get(handle)->get<ProcessDescriptor>();
			process->add_descriptor(descriptor);

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
			if (auto proc_desc = handle->get<ProcessDescriptor>()) {
				*frame->ret() = 0;

				auto guard = proc_desc->process.lock();
				if (!guard) {
					break;
				}
				(*guard)->killed = true;
				(*guard)->exit(-1, proc_desc);
			}
			else if (auto thread_desc = handle->get<ThreadDescriptor>()) {
				*frame->ret() = 0;

				auto guard = thread_desc->thread.lock();
				if (!guard) {
					break;
				}
				(*guard)->exited = true;
				(*guard)->exit(-1, thread_desc);
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
			if (auto proc_desc = handle->get<ProcessDescriptor>()) {
				auto guard = proc_desc->process.lock();
				if (!guard) {
					*frame->ret() = proc_desc->exit_status;
					break;
				}
				*frame->ret() = ERR_TRY_AGAIN;
			}
			else if (auto thread_desc = handle->get<ThreadDescriptor>()) {
				auto guard = thread_desc->thread.lock();
				if (!guard) {
					*frame->ret() = thread_desc->exit_status;
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

			println("[kernel]: thread ", thread->name, " sleeping for ", us, "us");
			thread->sleep_for(us);
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
								break;
							}

							*frame->ret() = 0;
							break;
						}
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
						GLOBAL_EVENT_QUEUE.produce_event.reset();
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
		default:
			println("[kernel]: invalid syscall ", num);
			*frame->ret() = ERR_INVALID_ARGUMENT;
			break;
	}
}
