#include "process.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "mem/vspace.hpp"
#include "sys/service.hpp"
#include "ipc.hpp"

Process::Process(kstd::string_view name, bool user)
	: name {name}, page_map {user ? &KERNEL_PROCESS->page_map : nullptr}, user {user} {
	usize start = 0x200000;
	vmem.init(start, 0x7FFFFFFFE000 - start, PAGE_SIZE);
}

Process::~Process() {
	if (service) {
		service_remove(this);
	}

	auto guard = mappings.lock();

	Mapping* next;
	for (Mapping* mapping = guard->get_first(); mapping; mapping = next) {
		next = static_cast<Mapping*>(mapping->hook.successor);

		auto base = mapping->base;
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);
			if (mapping->flags & MemoryAllocFlags::Backed) {
				pfree(page_phys, 1);
			}
		}
		vmem.xfree(base, mapping->size);

		guard->remove(mapping);
		delete mapping;
	}

	vmem.destroy(true);
}

usize Process::allocate(void* base, usize size, MemoryAllocFlags flags, UniqueKernelMapping* kernel_mapping, CacheMode cache_mode) {
	auto real_base = ALIGNDOWN(reinterpret_cast<usize>(base), PAGE_SIZE);
	size = ALIGNUP(size, PAGE_SIZE);

	if (!size) {
		return 0;
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
		auto page_flags = PageFlags::User;
		if (flags & MemoryAllocFlags::Read) {
			page_flags |= PageFlags::Read;
		}
		if (flags & MemoryAllocFlags::Write) {
			page_flags |= PageFlags::Write;
		}
		if (flags & MemoryAllocFlags::Execute) {
			page_flags |= PageFlags::Execute;
		}

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
		.flags = flags
	};
	mappings.lock()->insert(mapping);

	if (kernel_mapping) {
		*kernel_mapping = std::move(unique_kernel_mapping);
	}

	return virt;
}

void Process::free(usize ptr, usize) {
	auto guard = mappings.lock();

	auto mapping = guard->find<usize, &Mapping::base>(ptr);
	if (!mapping) {
		return;
	}
	auto base = mapping->base;

	if (mapping->flags & MemoryAllocFlags::Backed) {
		for (usize i = 0; i < mapping->size; i += PAGE_SIZE) {
			auto page_phys = page_map.get_phys(base + i);
			page_map.unmap(base + i);
			pfree(page_phys, 1);
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
}

void Process::add_thread(Thread* thread) {
	IrqGuard irq_guard {};
	threads.lock()->push(thread);
}

void Process::remove_thread(Thread* thread) {
	IrqGuard irq_guard {};
	threads.lock()->remove(thread);
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
	Mapping* next;
	for (Mapping* mapping = guard->get_first(); mapping; mapping = next) {
		if (addr >= mapping->base && addr < mapping->base + mapping->size) {
			println("[kernel]: handled pagefault at ", Fmt::Hex, addr, Fmt::Reset, " in process ", name);
			return true;
		}

		next = static_cast<Mapping*>(mapping->hook.successor);
	}

	return false;
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
		return std::move(ptr);
	}
}

ManuallyInit<Process> KERNEL_PROCESS;
