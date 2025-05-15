#include "process.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "sys/service.hpp"
#include "arch/cpu.hpp"
#include "ipc.hpp"
#include "limits.hpp"

ManuallyDestroy<Mutex<kstd::unordered_map<int, Process*>>> PID_TO_PROC;
static ManuallyDestroy<kstd::vector<int>> FREE_PIDS;
static bool USE_FREE_PIDS = false;
static int PID_COUNTER = 1;

Process::Process(kstd::string_view name, bool user, Handle&& stdin, Handle&& stdout, Handle&& stderr)
	: name {name}, page_map {user ? &KERNEL_PROCESS->page_map : nullptr}, user {user} {
	usize start = 0x200000;
	vmem.init(start, 0x7FFFFFFFE000 - start, PAGE_SIZE);

	handles.insert(std::move(stdin));
	handles.insert(std::move(stdout));
	handles.insert(std::move(stderr));

	if (user) {
		auto guard = PID_TO_PROC->lock();
		if (USE_FREE_PIDS && !FREE_PIDS->is_empty()) {
			pid = FREE_PIDS->pop().value();
		}
		else if (PID_COUNTER == kstd::numeric_limits<int>::max) {
			IrqGuard irq_guard {};
			println("[kernel][sched]: maximum number of processes reached");
			auto* cpu = get_current_thread()->cpu;
			cpu->scheduler.exit_process(-1);
		}
		else {
			pid = PID_COUNTER++;
			if (PID_COUNTER == kstd::numeric_limits<int>::max) {
				USE_FREE_PIDS = true;
			}
		}

		assert(!guard->get(pid));
		guard->insert(pid, this);
	}
}

Process::~Process() {
	if (service) {
		service_remove(this);
	}

	{
		auto pid_guard = PID_TO_PROC->lock();
		assert(pid_guard->get(pid));
		pid_guard->remove(pid);
		FREE_PIDS->push(pid);
	}

	auto guard = mappings.lock();

	Mapping* next;
	for (Mapping* mapping = guard->get_first(); mapping; mapping = next) {
		next = static_cast<Mapping*>(mapping->hook.successor);

		auto base = mapping->base;
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);
			if ((mapping->flags & MemoryAllocFlags::Backed) ||
				(mapping->flags & MemoryAllocFlags::Demand && page_phys)) {
				pfree(page_phys, 1);
			}
		}
		vmem.xfree(base, mapping->size);

		guard->remove(mapping);
		delete mapping;
	}

	vmem.destroy(true);
}

usize Process::allocate(
	void* base,
	usize size,
	PageFlags prot,
	MemoryAllocFlags flags,
	UniqueKernelMapping* kernel_mapping,
	CacheMode cache_mode) {
	auto real_base = ALIGNDOWN(reinterpret_cast<usize>(base), PAGE_SIZE);
	size = ALIGNUP(size, PAGE_SIZE);

	if (!size) {
		return 0;
	}

	prot |= PageFlags::User;

	if (flags & MemoryAllocFlags::Fixed) {
		auto guard = mappings.lock();

		usize i = 0;
		while (i < size) {
			auto node = guard->get_root();
			while (node) {
				auto addr = real_base + i;

				if (addr < node->base) {
					// NOLINTNEXTLINE
					node = guard->get_left(node);
				}
				else if (addr >= node->base + node->size) {
					// NOLINTNEXTLINE
					node = guard->get_right(node);
				}
				else {
					guard->remove(node);
					vmem.xfree(node->base, node->size);

					auto at_start = addr - node->base;
					if (at_start) {
						auto a = vmem.xalloc(at_start, node->base, node->base);
						assert(a);
						auto mapping = new Mapping {
							.base = node->base,
							.size = at_start,
							.prot = node->prot,
							.flags = node->flags
						};
						guard->insert(mapping);
					}

					auto range_end = real_base + size;
					auto node_end = node->base + node->size;

					if (range_end < node_end) {
						auto a = vmem.xalloc(node_end - range_end, range_end, range_end);
						assert(a);
						auto mapping = new Mapping {
							.base = range_end,
							.size = node_end - range_end,
							.prot = node->prot,
							.flags = node->flags
						};
						guard->insert(mapping);
					}

					usize to_unmap = kstd::min(node->size - at_start, size - i);
					if (node->flags & MemoryAllocFlags::Backed) {
						for (usize j = 0; j < to_unmap; j += PAGE_SIZE) {
							auto page_phys = page_map.get_phys(addr + j);
							page_map.unmap(addr + j);
							pfree(page_phys, 1);
						}
					}
					else if (node->flags & MemoryAllocFlags::Demand) {
						for (usize j = 0; j < to_unmap; j += PAGE_SIZE) {
							auto page_phys = page_map.get_phys(addr + j);
							page_map.unmap(addr + j);

							if (page_phys) {
								pfree(page_phys, 1);
							}
						}
					}

					i += node->size - at_start;
					delete node;
					break;
				}
			}
		}
	}

	auto virt = vmem.xalloc(size, real_base, real_base);
	if (!virt) {
		return 0;
	}

	UniqueKernelMapping unique_kernel_mapping {};
	usize kernel_virt = 0;
	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
	if (kernel_mapping) {
		auto* ptr = KERNEL_VSPACE.alloc(size);
		if (!ptr) {
			vmem.xfree(virt, size);
			return 0;
		}
		kernel_virt = reinterpret_cast<usize>(ptr);
		unique_kernel_mapping = {
			ptr,
			size
		};
	}

	if (flags & MemoryAllocFlags::Backed) {
		auto page_flags = PageFlags::User | prot;

		for (usize i = 0; i < size; i += PAGE_SIZE) {
			auto phys = pmalloc(1);
			if (!phys || !page_map.map(virt + i, phys, page_flags, cache_mode) ||
				(kernel_mapping && !KERNEL_MAP.map(kernel_virt + i, phys, PageFlags::Read | PageFlags::Write, CacheMode::WriteBack))) {

				for (usize j = 0; j < i; j += PAGE_SIZE) {
					auto page_phys = page_map.get_phys(virt + j);
					page_map.unmap(virt + j);
					pfree(page_phys, 1);
				}
				if (kernel_mapping) {
					for (usize j = 0; j < i; j += PAGE_SIZE) {
						KERNEL_MAP.unmap(kernel_virt + j);
					}
					KERNEL_VSPACE.free(reinterpret_cast<void*>(kernel_virt), size);
				}
				vmem.xfree(virt, size);
				return 0;
			}
		}
	}

	auto mapping = new Mapping {
		.base = virt,
		.size = size,
		.prot = prot,
		.flags = flags
	};
	mappings.lock()->insert(mapping);

	if (kernel_mapping) {
		*kernel_mapping = std::move(unique_kernel_mapping);
	}

	return virt;
}

bool Process::free(usize ptr, usize size) {
	size = ALIGNUP(size, PAGE_SIZE);

	auto guard = mappings.lock();

	auto mapping = guard->find<usize, &Mapping::base>(ptr);
	if (!mapping) {
		return false;
	}
	auto base = mapping->base;
	//assert(size == mapping->size);

	if (mapping->flags & MemoryAllocFlags::Backed) {
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);
			pfree(page_phys, 1);
		}
	}
	else if (mapping->flags & MemoryAllocFlags::Demand) {
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);

			if (page_phys) {
				pfree(page_phys, 1);
			}
		}
	}
	else {
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			page_map.unmap(base + i);
		}
	}
	vmem.xfree(base, mapping->size);

	guard->remove(mapping);
	delete mapping;
	return true;
}

bool Process::protect(usize ptr, usize size, PageFlags prot) {
	if (ptr & (PAGE_SIZE - 1) || (size & (PAGE_SIZE - 1))) {
		return false;
	}

	prot |= PageFlags::User;

	auto guard = mappings.lock();

	usize i = 0;
	while (i < size) {
		auto node = guard->get_root();
		while (node) {
			auto addr = ptr + i;

			if (addr < node->base) {
				// NOLINTNEXTLINE
				node = guard->get_left(node);
			}
			else if (addr >= node->base + node->size) {
				// NOLINTNEXTLINE
				node = guard->get_right(node);
			}
			else {
				if (node->base == addr && node->base + node->size == ptr + size && node->prot == prot) {
					i += node->size;
					break;
				}

				guard->remove(node);
				vmem.xfree(node->base, node->size);

				auto at_start = addr - node->base;
				if (at_start) {
					auto a = vmem.xalloc(at_start, node->base, node->base);
					assert(a);
					auto mapping = new Mapping {
						.base = node->base,
						.size = at_start,
						.prot = node->prot,
						.flags = node->flags
					};
					guard->insert(mapping);
				}

				auto range_end = ptr + size;
				auto node_end = node->base + node->size;

				if (range_end < node_end) {
					auto a = vmem.xalloc(node_end - range_end, range_end, range_end);
					assert(a);
					auto mapping = new Mapping {
						.base = range_end,
						.size = node_end - range_end,
						.prot = node->prot,
						.flags = node->flags
					};
					guard->insert(mapping);
				}

				usize to_protect = kstd::min(node->size - at_start, size - i);

				auto a = vmem.xalloc(to_protect, addr, addr);
				assert(a);

				node->base = addr;
				node->size = to_protect;
				node->prot = prot;
				for (usize j = 0; j < to_protect; j += PAGE_SIZE) {
					page_map.protect(addr + j, prot, CacheMode::WriteBack);
				}
				guard->insert(node);

				i += node->size - at_start;
				break;
			}
		}

		if (!node) {
			return false;
		}
	}

	return true;
}

void Process::add_thread(Thread* thread) {
	int tid;
	{
		IrqGuard irq_guard {};
		auto guard = threads.lock();
		thread->thread_id = ++max_thread_id;
		guard->push(thread);
		assert(thread->thread_id <= kstd::numeric_limits<int>::max);
		tid = static_cast<int>(thread->thread_id);
	}

	auto guard = tid_to_thread.lock();
	// todo fix
	assert(!guard->get(tid));
	guard->insert(tid, thread);
}

void Process::remove_thread(Thread* thread) {
	{
		IrqGuard irq_guard {};
		threads.lock()->remove(thread);
	}

	auto guard = tid_to_thread.lock();
	assert(guard->get(static_cast<int>(thread->thread_id)));
	guard->remove(static_cast<int>(thread->thread_id));
}

void Process::add_descriptor(ProcessDescriptor* descriptor) {
	IrqGuard irq_guard {};
	descriptors.lock()->push(descriptor);
}

void Process::remove_descriptor(ProcessDescriptor* descriptor) {
	IrqGuard irq_guard {};
	descriptors.lock()->remove(descriptor);
}

void Process::exit(int status, ProcessDescriptor* skip_lock) {
	IrqGuard irq_guard {};
	auto guard = descriptors.lock();
	for (auto& desc : *guard) {
		if (&desc != skip_lock) {
			auto proc_guard = desc.process.lock();
			assert(proc_guard == this);
			*proc_guard = nullptr;
		}
		else {
			assert(skip_lock->process.get_unsafe() == this);
			skip_lock->process.get_unsafe() = nullptr;
		}

		desc.exit_status = status;
	}
	guard->clear();
}

bool Process::handle_pagefault(usize addr) {
	auto guard = mappings.lock();

	addr = ALIGNDOWN(addr, PAGE_SIZE);

	auto node = guard->get_root();
	while (node) {
		if (addr < node->base) {
			// NOLINTNEXTLINE
			node = guard->get_left(node);
		}
		else if (addr >= node->base + node->size) {
			// NOLINTNEXTLINE
			node = guard->get_right(node);
		}
		else {
			if (node->flags & MemoryAllocFlags::Demand) {
				auto phys = page_map.get_phys(addr);
				if (phys) {
					return false;
				}

				assert(!phys);

				auto page = pmalloc(1);
				if (!page) {
					println("[kernel][sched]: failed to allocate demand-allocated page");
					return false;
				}

				if (!page_map.map(addr, page, node->prot, CacheMode::WriteBack)) {
					println("[kernel][sched]: failed to map demand-allocated page");
					pfree(page, 1);
					return false;
				}

				return true;
			}

			break;
		}
	}

	return false;
}

Process* Process::clone() {
	kstd::optional<Handle> old_std_handles[3] {
		handles.get(STDIN_HANDLE),
		handles.get(STDOUT_HANDLE),
		handles.get(STDERR_HANDLE)
	};

	Handle std_handles[3] {EmptyHandle {}, EmptyHandle {}, EmptyHandle {}};
	for (u32 i = 0; i < 3; ++i) {
		auto& handle = old_std_handles[i];

		kstd::shared_ptr<OpenFile>* ptr;
		if (!handle || !(ptr = handle->get<kstd::shared_ptr<OpenFile>>())) {
			continue;
		}

		auto cursor = (*ptr)->cursor;
		auto node = (*ptr)->node;

		auto new_node = kstd::make_shared<OpenFile>(node);
		new_node->cursor = cursor;

		std_handles[i] = std::move(new_node);
	}

	auto new_process = new Process {name, true, std::move(std_handles[0]), std::move(std_handles[1]), std::move(std_handles[2])};

	auto guard = mappings.lock();
	auto new_guard = new_process->mappings.lock();

	auto node = guard->get_first();
	while (node) {
		auto new_addr = new_process->vmem.xalloc(node->size, node->base, node->base);
		assert(new_addr);

		if (node->flags & MemoryAllocFlags::Backed) {
			if (node->flags & MemoryAllocFlags::Backed) {
				for (usize i = 0; i < node->size; i += PAGE_SIZE) {
					auto page_phys = page_map.get_phys(node->base + i);
					assert(page_phys);

					auto new_page = pmalloc(1);
					if (!new_page) {
						new_process->vmem.xfree(new_addr, node->size);
						delete new_process;
						return nullptr;
					}

					memcpy(to_virt<void>(new_page), to_virt<void>(page_phys), PAGE_SIZE);

					if (!new_process->page_map.map(node->base + i, new_page, node->prot, CacheMode::WriteBack)) {
						new_process->vmem.xfree(new_addr, node->size);
						pfree(new_page, 1);
						delete new_process;
						return nullptr;
					}
				}
			}
			else if (node->flags & MemoryAllocFlags::Demand) {
				for (usize i = 0; i < node->size; i += PAGE_SIZE) {
					auto page_phys = page_map.get_phys(node->base + i);
					if (!page_phys) {
						continue;
					}

					auto new_page = pmalloc(1);
					if (!new_page) {
						new_process->vmem.xfree(new_addr, node->size);
						delete new_process;
						return nullptr;
					}

					memcpy(to_virt<void>(new_page), to_virt<void>(page_phys), PAGE_SIZE);

					if (!new_process->page_map.map(node->base + i, new_page, node->prot, CacheMode::WriteBack)) {
						new_process->vmem.xfree(new_addr, node->size);
						pfree(new_page, 1);
						delete new_process;
						return nullptr;
					}
				}
			}
		}

		auto new_mapping = new Mapping {*node};

		new_guard->insert(new_mapping);

		// NOLINTNEXTLINE
		node = guard->get_successor(node);
	}

	return new_process;
}

UniqueKernelMapping::~UniqueKernelMapping() {
	if (ptr) {
		auto& KERNEL_MAP = KERNEL_PROCESS->page_map;
		for (usize i = 0; i < size; ++i) {
			KERNEL_MAP.unmap(reinterpret_cast<u64>(ptr) + i);
		}
		KERNEL_VSPACE.free(ptr, size);
	}
}

ProcessDescriptor::~ProcessDescriptor() {
	IrqGuard irq_guard {};
	auto guard = process.lock();
	if (guard) {
		(*guard)->remove_descriptor(this);
	}
}

kstd::shared_ptr<ProcessDescriptor> ProcessDescriptor::duplicate() {
	IrqGuard irq_guard {};
	auto guard = process.lock();
	if (!guard) {
		return kstd::make_shared<ProcessDescriptor>(nullptr, 0);
	}
	else {
		auto ptr = kstd::make_shared<ProcessDescriptor>(*guard, exit_status);
		(*guard)->add_descriptor(ptr.data());
		return ptr;
	}
}

ManuallyInit<Process> KERNEL_PROCESS;
