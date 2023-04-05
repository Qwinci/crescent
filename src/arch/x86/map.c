#include "arch/map.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"
#include "string.h"

static const usize X86_HUGEPAGE_SIZE = 0x200000;

#define X86_PF_P (1ULL << 0)
#define X86_PF_RW (1ULL << 1)
#define X86_PF_U (1ULL << 2)
#define X86_PF_WT (1ULL << 3)
#define X86_PF_CD (1ULL << 4)
#define X86_PF_A (1ULL << 5)
#define X86_PF_D (1ULL << 6)
#define X86_PF_H (1ULL << 7)
#define X86_PF_G (1ULL << 8)
#define X86_PF_PAT (1ULL << 7)
#define X86_PF_HUGE_PAT (1ULL << 12)
#define X86_PF_NX (1ULL << 63)

#define PAT0_I (0)
#define PAT1_I (X86_PF_WT)
#define PAT2_I (X86_PF_CD)
#define PAT3_I (X86_PF_CD | X86_PF_WT)
#define PAT4_I (X86_PF_PAT)
#define PAT4_HUGE_I (X86_PF_HUGE_PAT)
#define PAT5_I (X86_PF_PAT | X86_PF_WT)
#define PAT5_HUGE_I (X86_PF_HUGE_PAT | X86_PF_WT)
#define PAT6_I (X86_PF_PAT | X86_PF_CD)
#define PAT6_HUGE_I (X86_PF_HUGE_PAT | X86_PF_CD)
#define PAT7_I (X86_PF_PAT | X86_PF_CD | X86_PF_WT)
#define PAT7_HUGE_I (X86_PF_HUGE_PAT | X86_PF_CD | X86_PF_WT)

#define PAT_STRONG_UC_F PAT0_I
#define PAT_UC_F PAT1_I
#define PAT_WC_F PAT2_I
#define PAT_WT_F PAT3_I
#define PAT_WB_F PAT4_I
#define PAT_WB_HUGE_F PAT4_HUGE_I
#define PAT_WP_F PAT5_I
#define PAT_WP_HUGE_F PAT5_HUGE_I

static u64 x86_pf_from_generic(PageFlags flags) {
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

#define PAGE_ADDR_MASK 0x000FFFFFFFFFF000
#define PAGE_FLAG_MASK 0xFFF0000000000FFF

usize arch_get_hugepage_size() {
	return X86_HUGEPAGE_SIZE;
}

void* arch_create_map() {
	Page* page = pmalloc(1);
	void* virt = to_virt(page->phys);
	u64* map = (u64*) virt;
	for (usize i = 0; i < 512; ++i) {
		Page* frame_phys = pmalloc(1);
		assert(frame_phys);
		memset(to_virt(frame_phys->phys), 0, PAGE_SIZE);
		map[i] = X86_PF_P | X86_PF_RW | frame_phys->phys;
	}
	return page;
}

void arch_destroy_map(void* map) {
	// todo keep track of used pages for the current task
	pfree((Page*) map, 1);
}

static void x86_refresh_page(usize addr) {
	__asm__ volatile("invlpg [%0]" : : "r"(addr) : "memory");
}

void arch_map_page(void* map, usize virt, usize phys, PageFlags flags) {
	u64* pml4 = (u64*) to_virt(((Page*) map)->phys);

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

	u64* pt;
	if (pd[pd_offset] & X86_PF_P) {
		pd[pd_offset] &= ~PAGE_FLAG_MASK;
		pd[pd_offset] |= x86_generic_flags;
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
}

void arch_unmap_page(void* map, usize virt) {
	u64* pml4 = (u64*) to_virt(((Page*) map)->phys);

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
}

void arch_use_map(void* map) {
	__asm__ volatile("mov cr3, %0" : : "r"(((Page*) map)->phys) : "memory");
}

void* KERNEL_MAP = NULL;