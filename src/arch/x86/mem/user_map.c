#include "arch/x86/sched/x86_task.h"
#include "map.h"
#include "mem/page.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "sched/process.h"
#include "string.h"

void x86_add_map_page(X86PageMap* map, Page* page) {
	page->prev = NULL;
	page->next = map->map_pages;
	if (map->map_pages) {
		map->map_pages->prev = page;
	}
	map->map_pages = page;
}

void x86_remove_map_page(X86PageMap* map, Page* page) {
	if (page->prev) {
		page->prev->next = page->next;
	}
	else {
		map->map_pages = page->next;
	}
	if (page->next) {
		page->next->prev = page->prev;
	}
}

void arch_user_map_page(Process* process, usize virt, usize phys, PageFlags flags) {
	X86PageMap* map = (X86PageMap*) process->map;
	u64* pml4 = (u64*) to_virt(map->page->phys);

	u64 x86_flags = x86_pf_from_generic(flags);

	usize orig_virt = virt;

	virt >>= 12;
	u64 pt_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pd_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pdp_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pml4_offset = virt & 0x1FF;

	u64 x86_generic_flags = X86_PF_P | X86_PF_RW | (flags & PF_USER ? X86_PF_U : 0);

	u64* pdp;
	if (pml4[pml4_offset] & X86_PF_P) {
		pml4[pml4_offset] &= ~PAGE_FLAG_MASK;
		pml4[pml4_offset] |= x86_generic_flags;
		pdp = (u64*) to_virt(pml4[pml4_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		process_add_mapping(process, page->phys, PAGE_SIZE);
		x86_add_map_page(map, page);
		pdp = (u64*) to_virt(page->phys);
		memset(pdp, 0, PAGE_SIZE);

		pml4[pml4_offset] = page->phys | x86_generic_flags;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pdp[pdp_offset] &= ~PAGE_FLAG_MASK;
		pdp[pdp_offset] |= x86_generic_flags;
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		x86_add_map_page(map, page);
		pd = (u64*) to_virt(page->phys);
		memset(pd, 0, PAGE_SIZE);

		pdp[pdp_offset] = page->phys | x86_generic_flags;
	}

	if (flags & PF_HUGE) {
		pd[pd_offset] = phys | x86_flags | X86_PF_H;
		x86_refresh_page(orig_virt & ~(X86_HUGEPAGE_SIZE - 1));
		return;
	}
	if (pd[pd_offset] & X86_PF_H && flags & PF_SPLIT) {
		Page* page = pmalloc(1);
		assert(page);
		x86_add_map_page(map, page);
		u64* frame = (u64*) to_virt(page->phys);

		u64 old_flags = pd[pd_offset] & PAGE_FLAG_MASK;
		u64 old_addr = pd[pd_offset] & PAGE_ADDR_MASK;

		old_flags &= ~X86_PF_H;
		pd[pd_offset] = page->phys | x86_generic_flags;
		for (u16 i = 0; i < 512; ++i) {
			frame[i] = old_flags | (old_addr + i * PAGE_SIZE);
			x86_refresh_page((orig_virt & ~(PAGE_SIZE - 1)) + i * PAGE_SIZE);
		}
	}
	else if (pd[pd_offset] & X86_PF_H) {
		pd[pd_offset] &= ~PAGE_FLAG_MASK;
		pd[pd_offset] |= flags | X86_PF_H;
		x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
		return;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pd[pd_offset] &= ~PAGE_FLAG_MASK;
		pd[pd_offset] |= x86_generic_flags;
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		x86_add_map_page(map, page);
		pt = (u64*) to_virt(page->phys);
		memset(pt, 0, PAGE_SIZE);

		pd[pd_offset] = page->phys | x86_generic_flags;
	}

	pt[pt_offset] = phys | x86_flags;
	x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
}

void arch_user_unmap_page(Process* process, usize virt, bool dealloc) {
	X86PageMap* map = (X86PageMap*) process->map;
	u64* pml4 = (u64*) to_virt(map->page->phys);

	usize orig_virt = virt;

	virt >>= 12;
	u64 pt_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pd_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pdp_offset = virt & 0x1FF;
	virt >>= 9;
	u64 pml4_offset = virt & 0x1FF;

	u64* pdp;
	if (pml4[pml4_offset] & X86_PF_P) {
		pdp = (u64*) to_virt(pml4[pml4_offset] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	if (pd[pd_offset] & X86_PF_H) {
		pd[pd_offset] = 0;
		x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
		if (dealloc) {
			goto dealloc_huge;
		}
		return;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	pt[pt_offset] = 0;
	x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));

	if (!dealloc) {
		return;
	}

	for (usize i = 0; i < 512; ++i) {
		if (pt[i]) {
			return;
		}
	}

	usize pd_phys = pd[pd_offset] & PAGE_ADDR_MASK;
	usize pd_virt = (usize) to_virt(pd_phys);
	pd[pd_offset] = 0;
	x86_refresh_page(pd_virt);
	Page* page = page_from_addr(pd_phys);
	x86_remove_map_page(map, page);
	pfree(page, 1);

dealloc_huge:
	for (usize i = 0; i < 512; ++i) {
		if (pd[i]) {
			return;
		}
	}

	usize pdp_phys = pdp[pdp_offset] & PAGE_ADDR_MASK;
	usize pdp_virt = (usize) to_virt(pdp_phys);
	pdp[pdp_offset] = 0;
	x86_refresh_page(pdp_virt);

	page = page_from_addr(pdp_phys);
	x86_remove_map_page(map, page);
	pfree(page, 1);
}