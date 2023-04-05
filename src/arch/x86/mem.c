#include "arch/map.h"
#include "limine/limine.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"

usize HHDM_OFFSET = 0;

static volatile struct limine_memmap_request MMAP_REQUEST = {
	.id = LIMINE_MEMMAP_REQUEST
};

static volatile struct limine_hhdm_request HHDM_REQUEST = {
	.id = LIMINE_HHDM_REQUEST
};

static volatile struct limine_kernel_address_request KERNEL_ADDRESS_REQUEST = {
	.id = LIMINE_KERNEL_ADDRESS_REQUEST
};

#define SIZE_2MB 0x200000
#define SIZE_4GB 0x100000000

void x86_init_mem() {
	HHDM_OFFSET = HHDM_REQUEST.response->offset;

	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		struct limine_memmap_entry* entry = MMAP_REQUEST.response->entries[i];
		if (entry->type == LIMINE_MEMMAP_USABLE) {
			pmalloc_add_mem((void*) entry->base, entry->length);
		}
	}

	KERNEL_MAP = arch_create_map();

	usize kernel_virt = KERNEL_ADDRESS_REQUEST.response->virtual_base;
	usize kernel_phys = KERNEL_ADDRESS_REQUEST.response->physical_base;

	struct limine_memmap_entry* max_phys_entry = MMAP_REQUEST.response->entries[MMAP_REQUEST.response->entry_count - 1];
	usize max_phys = max_phys_entry->base + max_phys_entry->length;

	usize aligned_max_phys = ALIGNUP(max_phys, SIZE_2MB);

	// todo
	// vm kernel init (HHDM_OFFSET + aligned_max_phys, 0xFFFFFFFF80000000 - (HHDM_OFFSET + aligned_max_phys))

	for (usize i = 0; i < SIZE_4GB; i += SIZE_2MB) {
		arch_map_page(KERNEL_MAP, (usize) to_virt(i), i, PF_READ | PF_WRITE | PF_HUGE);
	}

	for (usize i = 0; i < MMAP_REQUEST.response->entry_count; ++i) {
		struct limine_memmap_entry* entry = MMAP_REQUEST.response->entries[i];

		usize align = entry->base & (PAGE_SIZE - 1);
		usize size = entry->length + align;
		if (align) {
			entry->base &= ~(PAGE_SIZE - 1);
		}

		if (entry->type == LIMINE_MEMMAP_FRAMEBUFFER) {
			usize j = 0;
			while (j < size) {
				usize phys = entry->base + j;
				// todo WC
				arch_map_page(KERNEL_MAP, (usize) to_virt(phys), phys, PF_READ | PF_WRITE | PF_NC | PF_SPLIT);
				j += PAGE_SIZE;
			}
			continue;
		}

		usize j = 0;
		while ((entry->base + j) & (SIZE_2MB - 1) && j < size) {
			usize phys = entry->base + j;
			arch_map_page(KERNEL_MAP, (usize) to_virt(phys), phys, PF_READ | PF_WRITE);
			j += PAGE_SIZE;
		}

		while (j < size) {
			usize phys = entry->base + j;
			arch_map_page(KERNEL_MAP, (usize) to_virt(phys), phys, PF_READ | PF_WRITE | PF_HUGE);
			j += SIZE_2MB;
		}
	}

	extern char RODATA_START[];
	extern char RODATA_END[];
	extern char TEXT_START[];
	extern char TEXT_END[];
	extern char DATA_START[];
	extern char DATA_END[];

	const usize RODATA_SIZE = RODATA_END - RODATA_START;
	const usize RODATA_START_OFF = RODATA_START - (char*) kernel_virt;

	const usize TEXT_SIZE = TEXT_END - TEXT_START;
	const usize TEXT_START_OFF = TEXT_START - (char*) kernel_virt;

	const usize DATA_SIZE = DATA_END - DATA_START;
	const usize DATA_START_OFF = DATA_START - (char*) kernel_virt;

	for (usize i = RODATA_START_OFF; i < RODATA_START_OFF + RODATA_SIZE; i += PAGE_SIZE) {
		arch_map_page(KERNEL_MAP, kernel_virt + i, kernel_phys + i, PF_READ);
	}

	for (usize i = TEXT_START_OFF; i < TEXT_START_OFF + TEXT_SIZE; i += PAGE_SIZE) {
		arch_map_page(KERNEL_MAP, kernel_virt + i, kernel_phys + i, PF_READ | PF_EXEC);
	}

	for (usize i = DATA_START_OFF; i < DATA_START_OFF + DATA_SIZE; i += PAGE_SIZE) {
		arch_map_page(KERNEL_MAP, kernel_virt + i, kernel_phys + i, PF_READ | PF_WRITE);
	}

	arch_use_map(KERNEL_MAP);
}