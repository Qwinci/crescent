#pragma once
#include "types.h"

typedef enum {
	/// Allow reading from the page
	PF_READ = 1 << 0,
	/// Allow writing to the page
	PF_WRITE = 1 << 1,
	/// Allow executing from the page
	PF_EXEC = 1 << 2,
	/// Map a huge page
	PF_HUGE = 1 << 3,
	/// Disable caching for the page
	PF_NC = 1 << 4,
	/// Allow to split existing huge page into smaller ones
	PF_SPLIT = 1 << 5,
	/// Allow merging existing smaller pages into one huge one
	PF_MERGE = 1 << 6
} PageFlags;

usize arch_get_hugepage_size();
void* arch_create_map();
void arch_map_page(void* map, usize virt, usize phys, PageFlags flags);
void arch_unmap_page(void* map, usize virt);