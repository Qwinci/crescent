#include <cstring.hpp>
#include "arch/paging.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"
#include "cmath.hpp"

constexpr u64 FLAG_PRESENT = 0b1;
constexpr u64 FLAG_4KB = 0b10;
constexpr u16 FLAG_ACCESS = 1U << 10;
// Unprivileged execute never
constexpr u64 FLAG_UXN = 1ULL << 54;
// Privileged execute never
constexpr u64 FLAG_PXN = 1ULL << 53;
constexpr u64 TABLE_PRESENT = 0b11;

constexpr u8 AP_RW = 0b01;
constexpr u8 AP_PRIVILEGED_R = 0b10;
constexpr u8 AP_R = 0b11;

constexpr u64 PAGE_ADDR_MASK = 0x000FFFFFFFFFF000;

bool PageMap::map_1gb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	auto guard = lock.lock();

	u64 real_flags = 0;
	u16 ap = 0;
	if (flags & PageFlags::User) {
		if (flags & PageFlags::Write) {
			ap = AP_RW;
		}
		else if (flags & PageFlags::Read) {
			ap = AP_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_UXN;
		}
	}
	else {
		if (!(flags & PageFlags::Write)) {
			ap = AP_PRIVILEGED_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_PXN;
		}
	}

	virt >>= 30;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = to_virt<u64>(level0[level0_index] & PAGE_ADDR_MASK);
	}
	else {
		u64 page_phys = pmalloc(1);
		if (!page_phys) {
			return false;
		}
		used_pages.push(Page::from_phys(page_phys));

		level1 = to_virt<u64>(page_phys);
		memset(level1, 0, PAGE_SIZE);
		level0[level0_index] = page_phys | TABLE_PRESENT;
	}

	// MAIR_EL1 index
	u8 idx = static_cast<u8>(cache_mode);

	// outer shareable
	u16 sh = 0b10;

	level1[level1_index] = phys | FLAG_ACCESS | sh << 8 | ap << 6 | idx << 2 | FLAG_PRESENT | real_flags;

	return true;
}

bool PageMap::map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	auto guard = lock.lock();

	u64 real_flags = 0;
	u16 ap = 0;
	if (flags & PageFlags::User) {
		if (flags & PageFlags::Write) {
			ap = AP_RW;
		}
		else if (flags & PageFlags::Read) {
			ap = AP_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_UXN;
		}
	}
	else {
		if (!(flags & PageFlags::Write)) {
			ap = AP_PRIVILEGED_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_PXN;
		}
	}

	virt >>= 12;
	u64 level3_index = virt & 0x1FF;
	virt >>= 9;
	u64 level2_index = virt & 0x1FF;
	virt >>= 9;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = to_virt<u64>(level0[level0_index] & PAGE_ADDR_MASK);
	}
	else {
		u64 page_phys = pmalloc(1);
		if (!page_phys) {
			return false;
		}
		used_pages.push(Page::from_phys(page_phys));

		level1 = to_virt<u64>(page_phys);
		memset(level1, 0, 0x1000);

		level0[level0_index] = page_phys | TABLE_PRESENT;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		u64 page_phys = pmalloc(1);
		if (!page_phys) {
			return false;
		}
		used_pages.push(Page::from_phys(page_phys));

		level2 = to_virt<u64>(page_phys);
		memset(level2, 0, 0x1000);

		level1[level1_index] = page_phys | TABLE_PRESENT;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		u64 page_phys = pmalloc(1);
		if (!page_phys) {
			return false;
		}
		used_pages.push(Page::from_phys(page_phys));

		level3 = to_virt<u64>(page_phys);
		memset(level3, 0, 0x1000);

		level2[level2_index] = page_phys | TABLE_PRESENT;
	}

	// MAIR_EL1 index
	u8 idx = static_cast<u8>(cache_mode);

	// outer shareable
	u16 sh = 0b10;

	level3[level3_index] = phys | FLAG_ACCESS | sh << 8 | ap << 6 | idx << 2 | FLAG_4KB | FLAG_PRESENT | real_flags;

	return true;
}

void PageMap::unmap(u64 virt) {
	auto guard = lock.lock();

	auto orig_virt = virt;
	virt >>= 12;
	u64 level3_index = virt & 0x1FF;
	virt >>= 9;
	u64 level2_index = virt & 0x1FF;
	virt >>= 9;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = to_virt<u64>(level0[level0_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		if (!(level1[level1_index] & 0b10)) {
			// todo 1gb huge page
			return;
		}

		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (!(level2[level2_index] & 0b10)) {
			// todo 2mb huge page
			return;
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	orig_virt &= ~0xFFF;

	level3[level3_index] = 0;

	asm volatile("dsb ishst; tlbi vaae1, %0; dsb sy; isb" : : "r"(orig_virt >> 12) : "memory");
}

void PageMap::protect(u64 virt, PageFlags flags, CacheMode cache_mode) {
	auto guard = lock.lock();

	u64 real_flags = 0;
	u16 ap = 0;
	if (flags & PageFlags::User) {
		if (flags & PageFlags::Write) {
			ap = AP_RW;
		}
		else if (flags & PageFlags::Read) {
			ap = AP_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_UXN;
		}
	}
	else {
		if (!(flags & PageFlags::Write)) {
			ap = AP_PRIVILEGED_R;
		}

		if (!(flags & PageFlags::Execute)) {
			real_flags |= FLAG_PXN;
		}
	}

	// MAIR_EL1 index
	u8 idx = static_cast<u8>(cache_mode);
	// outer shareable
	u16 sh = 0b10;

	auto orig_virt = virt;
	virt >>= 12;
	u64 level3_index = virt & 0x1FF;
	virt >>= 9;
	u64 level2_index = virt & 0x1FF;
	virt >>= 9;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = to_virt<u64>(level0[level0_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		if (!(level1[level1_index] & 0b10)) {
			// todo 1gb huge page
			return;
		}

		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (!(level2[level2_index] & 0b10)) {
			// todo 2mb huge page
			return;
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	orig_virt &= ~0xFFF;

	level3[level3_index] &= PAGE_ADDR_MASK;
	level3[level3_index] |= FLAG_ACCESS | sh << 8 | ap << 6 | idx << 2 | FLAG_4KB | FLAG_PRESENT | real_flags;

	asm volatile("dsb ishst; tlbi vaae1, %0; dsb sy; isb" : : "r"(orig_virt >> 12) : "memory");
}

u64 PageMap::get_phys(u64 virt) {
	auto guard = lock.lock();

	u64 orig_virt = virt;

	auto offset = virt & 0xFFF;
	virt >>= 12;
	u64 level3_index = virt & 0x1FF;
	virt >>= 9;
	u64 level2_index = virt & 0x1FF;
	virt >>= 9;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = to_virt<u64>(level0[level0_index] & PAGE_ADDR_MASK);
	}
	else {
		return 0;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		if (!(level1[level1_index] & 0b10)) {
			return (level1[level1_index] & PAGE_ADDR_MASK) | (orig_virt & 0x3FFFFFFF);
		}

		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return 0;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (!(level2[level2_index] & 0b10)) {
			return (level2[level2_index] & PAGE_ADDR_MASK) | (orig_virt & 0x1FFFFF);
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return 0;
	}

	return (level3[level3_index] & PAGE_ADDR_MASK) | offset;
}

void PageMap::use() {
	asm volatile("msr TTBR0_EL1, %0; tlbi VMALLE1; dsb ISH; isb;" : : "r"(to_phys(level0)) : "memory");
}

PageMap::PageMap(PageMap* kernel_map) {
	auto phys = pmalloc(1);
	assert(phys);
	level0 = to_virt<u64>(phys);
	memset(level0, 0, PAGE_SIZE);
	used_pages.push(Page::from_phys(phys));

	if (kernel_map) {
		memcpy(level0 + 256, kernel_map->level0 + 256, PAGE_SIZE / 2);
	}
}

PageMap::PageMap(u64* early_map) {
	level0 = early_map;
}

PageMap::~PageMap() {
	for (auto& page : used_pages) {
		pfree(page.phys, 1);
	}
	used_pages.clear();
}

void PageMap::fill_high_half() {
	for (int i = 0; i < 512; ++i) {
		if (level0[i]) {
			continue;
		}

		auto phys = pmalloc(1);
		assert(phys);
		memset(to_virt<void>(phys), 0, PAGE_SIZE);
		level0[i] = phys | TABLE_PRESENT;
		used_pages.push(Page::from_phys(phys));
	}
}

ManuallyInit<PageMap> KERNEL_MAP;
