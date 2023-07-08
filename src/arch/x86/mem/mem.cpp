#include "mem.hpp"
#include "mem/page_allocator.hpp"
#include "limine/limine.h"
#include "arch/map.hpp"

static volatile limine_memmap_request MMAP_REQUEST {
	.id = LIMINE_MEMMAP_REQUEST
};

static volatile limine_hhdm_request HHDM_REQUEST {
	.id = LIMINE_HHDM_REQUEST
};

usize HHDM_OFFSET = 0;

void* arch_to_virt(usize phys) {
	return reinterpret_cast<void*>(HHDM_OFFSET + phys);
}

usize arch_to_phys(void* virt) {
	return reinterpret_cast<usize>(virt) - HHDM_OFFSET;
}

void x86_init_mem() {
	HHDM_OFFSET = HHDM_REQUEST.response->offset;

	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		auto entry = MMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			PAGE_ALLOCATOR.add_mem(entry->base, arch_to_virt(entry->base), entry->length);
		}
	}
}
