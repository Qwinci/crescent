#include "iospace.hpp"
#include "sched/process.hpp"
#include "vspace.hpp"

bool IoSpace::map(CacheMode cache_mode) {
	base = reinterpret_cast<usize>(KERNEL_VSPACE.alloc(size));
	if (!base) {
		println("failed to allocate mapping for io space");
		return false;
	}

	usize align = phys & (PAGE_SIZE - 1);
	usize aligned_phys = phys & ~(PAGE_SIZE - 1);

	auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

	for (usize i = 0; i < size + align; i += PAGE_SIZE) {
		auto virt = base + i;
		if (!KERNEL_MAP.map(virt, aligned_phys + i, PageFlags::Read | PageFlags::Write, cache_mode)) {
			println("kernel map failed");
			for (usize j = 0; j < i; j += PAGE_SIZE) {
				KERNEL_MAP.unmap(base + j);
			}
			return false;
		}
	}

	base += align;

	return true;
}
