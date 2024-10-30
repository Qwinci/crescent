#include "vspace.hpp"
#include "arch/paging.hpp"
#include "mem.hpp"
#include "pmalloc.hpp"
#include "sched/process.hpp"

VirtualSpace KERNEL_VSPACE;

void VirtualSpace::init(usize base, usize size) {
	vmem.init(base, size, PAGE_SIZE);
}

void VirtualSpace::destroy() {
	IrqGuard irq_guard {};
	vmem.destroy(true);
}

void* VirtualSpace::alloc(usize size) {
	IrqGuard irq_guard {};
	auto ret = vmem.xalloc(size, 0, 0);
	if (!ret) {
		return nullptr;
	}

	auto* reg = new Region {};
	reg->base = ret;
	reg->size = size;
	regions.lock()->insert(reg);

	return reinterpret_cast<void*>(ret);
}

void VirtualSpace::free(void* ptr, usize size) {
	IrqGuard irq_guard {};
	vmem.xfree(reinterpret_cast<usize>(ptr), size);

	Region* reg;
	{
		auto guard = regions.lock();
		reg = guard->find<usize, &Region::base>(reinterpret_cast<usize>(ptr));
		guard->remove(reg);
	}

	assert(reg->size == size);
	delete reg;
}

void* VirtualSpace::alloc_backed(usize size, PageFlags flags, CacheMode cache_mode) {
	auto vm = alloc(size);
	if (!vm) {
		return nullptr;
	}

	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	auto pages = ALIGNUP(size, PAGE_SIZE) / PAGE_SIZE;
	for (usize i = 0; i < pages; ++i) {
		auto virt = reinterpret_cast<u64>(vm) + i * PAGE_SIZE;

		auto phys = pmalloc(1);
		if (!phys) {
			goto cleanup;
		}

		if (!KERNEL_MAP.map(virt, phys, flags, cache_mode)) {
			goto cleanup;
		}

		continue;

		cleanup:
		if (phys) {
			pfree(phys, 1);
		}
		for (usize j = 0; j < i; ++j) {
			virt = reinterpret_cast<u64>(vm) + j * PAGE_SIZE;
			auto p = KERNEL_MAP.get_phys(virt);
			KERNEL_MAP.unmap(virt);
			pfree(p, 1);
		}

		free(vm, size);
		return nullptr;
	}

	return vm;
}

void VirtualSpace::free_backed(void* ptr, usize size) {
	auto aligned = ALIGNUP(size, PAGE_SIZE);

	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < aligned; i += PAGE_SIZE) {
		auto virt = reinterpret_cast<u64>(ptr) + i;
		auto phys = KERNEL_MAP.get_phys(virt);
		KERNEL_MAP.unmap(virt);
		pfree(phys, 1);
	}

	free(ptr, size);
}
