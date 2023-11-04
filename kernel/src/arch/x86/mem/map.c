#include "arch/map.h"
#include "map.h"
#include "mem/allocator.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "string.h"

u64 x86_pf_from_generic(PageFlags flags) {
	u64 x86_flags = 0;
	if (flags & PF_READ) {
		x86_flags |= X86_PF_P;
	}
	if (flags & PF_WRITE) {
		x86_flags |= X86_PF_RW;
	}
	if (!(flags & PF_EXEC)) {
		x86_flags |= X86_PF_NX;
	}
	if (flags & PF_USER) {
		x86_flags |= X86_PF_U;
	}
	if (flags & PF_WT) {
		x86_flags |= X86_PF_WT;
	}
	if (flags & PF_NC) {
		x86_flags |= X86_PF_CD;
	}
	return x86_flags;
}

usize arch_get_hugepage_size() {
	return X86_HUGEPAGE_SIZE;
}

void* arch_create_user_map() {
	Page* page = pmalloc(1);
	if (!page) {
		return NULL;
	}
	u64* virt = (u64*) to_virt(page->phys);
	memset(virt, 0, 256 * 8);
	memcpy(virt + 256, to_virt(((X86PageMap*) KERNEL_MAP)->page->phys + 256 * 8), 256 * 8);

	X86PageMap* map = kmalloc(sizeof(X86PageMap));
	if (!map) {
		pfree(page, 1);
		return NULL;
	}
	map->ref_count = 0;
	map->page = page;
	map->lock = (Mutex) {};
	map->map_pages = NULL;

	return map;
}

void* arch_create_map() {
	Page* page = pmalloc(1);
	assert(page);
	u64* virt = (u64*) to_virt(page->phys);
	memset(virt, 0, PAGE_SIZE);
	for (usize i = 256; i < 512; ++i) {
		Page* frame_phys = pmalloc(1);
		assert(frame_phys);
		memset(to_virt(frame_phys->phys), 0, PAGE_SIZE);
		virt[i] = X86_PF_P | X86_PF_RW | frame_phys->phys;
	}

	X86PageMap* map = kmalloc(sizeof(X86PageMap));
	assert(map);
	map->page = page;
	map->ref_count = 1;
	map->lock = (Mutex) {};
	map->map_pages = NULL;

	return map;
}

void arch_destroy_map(void* map) {
	X86PageMap* self = (X86PageMap*) map;
	mutex_lock(&self->lock);
	if (self->ref_count > 1) {
		--self->ref_count;
		mutex_unlock(&self->lock);
		return;
	}
	for (Page* page = self->map_pages; page;) {
		Page* next = page->next;
		pfree(page, 1);
		page = next;
	}
	pfree(self->page, 1);
	mutex_unlock(&self->lock);
	kfree(self, sizeof(X86PageMap));
}

void x86_refresh_page(usize addr) {
	__asm__ volatile("invlpg (%0)" : : "r"(addr) : "memory");
}

void arch_protect_page(void* map, usize virt, PageFlags flags) {
	X86PageMap* self = (X86PageMap*) map;
	mutex_lock(&self->lock);

	u64* pml4 = (u64*) to_virt(self->page->phys);

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

	u64* pdp;
	if (pml4[pml4_offset] & X86_PF_P) {
		pdp = (u64*) to_virt(pml4[pml4_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return;
	}

	if ((flags & PF_HUGE) || (pd[pd_offset] & X86_PF_H)) {
		pd[pd_offset] &= ~PAGE_FLAG_MASK;
		pd[pd_offset] |= x86_flags | X86_PF_H;
		x86_refresh_page(orig_virt & ~(X86_HUGEPAGE_SIZE - 1));
		mutex_unlock(&self->lock);
		return;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return;
	}

	pt[pt_offset] &= ~PAGE_FLAG_MASK;
	pt[pt_offset] |= x86_flags;
	x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
	mutex_unlock(&self->lock);
}

void arch_map_page(void* map, usize virt, usize phys, PageFlags flags) {
	X86PageMap* self = (X86PageMap*) map;
	mutex_lock(&self->lock);
	u64* pml4 = (u64*) to_virt(self->page->phys);

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
		pdp = (u64*) to_virt(pml4[pml4_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		pdp = (u64*) to_virt(page->phys);
		memset(pdp, 0, PAGE_SIZE);

		pml4[pml4_offset] = page->phys | x86_generic_flags;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		pd = (u64*) to_virt(page->phys);
		memset(pd, 0, PAGE_SIZE);

		pdp[pdp_offset] = page->phys | x86_generic_flags;
	}

	if (flags & PF_HUGE) {
		pd[pd_offset] = phys | x86_flags | X86_PF_H;
		x86_refresh_page(orig_virt & ~(X86_HUGEPAGE_SIZE - 1));
		mutex_unlock(&self->lock);
		return;
	}
	if (pd[pd_offset] & X86_PF_H && flags & PF_SPLIT) {
		Page* page = pmalloc(1);
		assert(page);
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
		mutex_unlock(&self->lock);
		return;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		Page* page = pmalloc(1);
		assert(page);
		pt = (u64*) to_virt(page->phys);
		memset(pt, 0, PAGE_SIZE);

		pd[pd_offset] = page->phys | x86_generic_flags;
	}

	pt[pt_offset] = phys | x86_flags;
	x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
	mutex_unlock(&self->lock);
}

void arch_unmap_page(void* map, usize virt, bool dealloc) {
	X86PageMap* self = (X86PageMap*) map;
	mutex_lock(&self->lock);
	u64* pml4 = (u64*) to_virt(self->page->phys);

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
		mutex_unlock(&self->lock);
		return;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return;
	}

	if (pd[pd_offset] & X86_PF_H) {
		pd[pd_offset] = 0;
		x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));
		if (dealloc) {
			goto dealloc_huge;
		}
		mutex_unlock(&self->lock);
		return;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return;
	}

	pt[pt_offset] = 0;
	x86_refresh_page(orig_virt & ~(PAGE_SIZE - 1));

	if (!dealloc) {
		mutex_unlock(&self->lock);
		return;
	}

	for (usize i = 0; i < 512; ++i) {
		if (pt[i]) {
			mutex_unlock(&self->lock);
			return;
		}
	}

	usize pd_phys = pd[pd_offset] & PAGE_ADDR_MASK;
	usize pd_virt = (usize) to_virt(pd_phys);
	pd[pd_offset] = 0;
	x86_refresh_page(pd_virt);
	pfree(page_from_addr(pd_phys), 1);

dealloc_huge:
	for (usize i = 0; i < 512; ++i) {
		if (pd[i]) {
			mutex_unlock(&self->lock);
			return;
		}
	}

	usize pdp_phys = pdp[pdp_offset] & PAGE_ADDR_MASK;
	usize pdp_virt = (usize) to_virt(pdp_phys);
	pdp[pdp_offset] = 0;
	x86_refresh_page(pdp_virt);
	pfree(page_from_addr(pdp_phys), 1);
	mutex_unlock(&self->lock);
}

usize arch_virt_to_phys(void* map, usize virt) {
	X86PageMap* self = (X86PageMap*) map;
	mutex_lock(&self->lock);

	u64* pml4 = (u64*) to_virt(self->page->phys);

	u64 p_offset = virt & 0xFFF;
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
		mutex_unlock(&self->lock);
		return 0;
	}

	u64* pd;
	if (pdp[pdp_offset] & X86_PF_P) {
		pd = (u64*) to_virt(pdp[pdp_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return 0;
	}

	if (pd[pd_offset] & X86_PF_H) {
		mutex_unlock(&self->lock);
		return pd[pd_offset] & PAGE_ADDR_MASK;
	}

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pt = (u64*) to_virt(pd[pd_offset] & PAGE_ADDR_MASK);
	}
	else {
		mutex_unlock(&self->lock);
		return 0;
	}

	usize addr = (pt[pt_offset] & PAGE_ADDR_MASK) + p_offset;
	mutex_unlock(&self->lock);
	return addr;
}

void arch_use_map(void* map) {
	__asm__ volatile("mov %0, %%cr3" : : "r"(((X86PageMap*) map)->page->phys) : "memory");
}

void* KERNEL_MAP = NULL;