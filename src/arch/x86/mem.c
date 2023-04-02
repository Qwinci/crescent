#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "limine/limine.h"

usize HHDM_OFFSET = 0;

static volatile struct limine_memmap_request MMAP_REQUEST = {
	.id = LIMINE_MEMMAP_REQUEST
};

static volatile struct limine_hhdm_request HHDM_REQUEST = {
	.id = LIMINE_HHDM_REQUEST
};

void x86_init_mem() {
	HHDM_OFFSET = HHDM_REQUEST.response->offset;

	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		struct limine_memmap_entry* entry = MMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			pmalloc_add_mem((void*) entry->base, entry->length);
		}
	}
}