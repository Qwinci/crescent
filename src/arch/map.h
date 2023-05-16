#pragma once
#include "types.h"

typedef enum {
	/// Allow reading from the page
	PF_READ = 1 << 0,
	/// Allow writing to the page
	PF_WRITE = 1 << 1,
	/// Allow executing from the page
	PF_EXEC = 1 << 2,
	/// Allow userspace to access the page
	PF_USER = 1 << 3,
	/// Map a huge page
	PF_HUGE = 1 << 4,
	/// Disable caching for the page
	PF_NC = 1 << 5,
	/// Allow combining writes for the page
	PF_WC = 1 << 6,
	/// Allow writing through cache for the page
	PF_WT = 1 << 7,
	/// Allow cache write back for the page
	PF_WB = 1 << 8,
	/// Allow to split existing huge page into smaller ones
	PF_SPLIT = 1 << 9
} PageFlags;

usize arch_get_hugepage_size();
void* arch_create_map();
void arch_destroy_map(void* map);
void arch_map_page(void* map, usize virt, usize phys, PageFlags flags);
void arch_protect_page(void* map, usize virt, PageFlags flags);
void arch_unmap_page(void* map, usize virt, bool dealloc);
void arch_use_map(void* map);
usize arch_virt_to_phys(void* map, usize virt);

typedef struct Task Task;

void arch_user_map_page(Task* task, usize virt, usize phys, PageFlags flags);
void arch_user_unmap_page(Task* task, usize virt, bool dealloc);

extern void* KERNEL_MAP;
extern void* CUR_MAP;