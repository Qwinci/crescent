#include <cstring.hpp>
#include "arch/paging.hpp"
#include "mem/mem.hpp"
#include "mem/pmalloc.hpp"

constexpr u64 FLAG_PRESENT = 0b1;
constexpr u64 FLAG_RW = 1U << 1;
constexpr u64 FLAG_USER = 1U << 2;
constexpr u64 FLAG_WT = 1U << 3;
constexpr u64 FLAG_CD = 1U << 4;
constexpr u64 FLAG_HUGE = 1U << 7;
constexpr u64 FLAG_GLOBAL = 1U << 8;
constexpr u64 FLAG_PAT = 1U << 7;
constexpr u64 FLAG_HUGE_PAT = 1U << 12;
constexpr u64 FLAG_NX = 1ULL << 63;

constexpr u64 PAGE_ADDR_MASK = 0x000FFFFFFFFFF000;

// todo lock

bool PageMap::map_2mb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	u64 all_flags = FLAG_PRESENT | FLAG_RW | (flags & PageFlags::User ? FLAG_USER : 0);

	virt >>= 21;
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
		memset(level1, 0, PAGE_SIZE);
		level0[level0_index] = page_phys | all_flags;
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
		memset(level2, 0, PAGE_SIZE);
		level1[level1_index] = page_phys | all_flags;
	}

	u64 real_flags = FLAG_HUGE;
	if (flags & PageFlags::Read) {
		real_flags |= FLAG_PRESENT;
	}
	if (flags & PageFlags::Write) {
		real_flags |= FLAG_RW;
	}
	if (!(flags & PageFlags::Execute)) {
		real_flags |= FLAG_NX;
	}
	if (flags & PageFlags::User) {
		real_flags |= FLAG_USER;
	}

	switch (cache_mode) {
		case CacheMode::WriteBack:
			break;
		case CacheMode::WriteCombine:
			real_flags |= FLAG_WT;
			break;
		case CacheMode::WriteThrough:
			real_flags |= FLAG_CD;
			break;
		case CacheMode::Uncached:
			real_flags |= FLAG_WT | FLAG_CD;
			break;
	}

	level2[level2_index] = phys | real_flags;

	return true;
}

bool PageMap::map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	u64 all_flags = FLAG_PRESENT | FLAG_RW | (flags & PageFlags::User ? FLAG_USER : 0);

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
		memset(level1, 0, PAGE_SIZE);
		level0[level0_index] = page_phys | all_flags;
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
		memset(level2, 0, PAGE_SIZE);
		level1[level1_index] = page_phys | all_flags;
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
		memset(level3, 0, PAGE_SIZE);
		level2[level2_index] = page_phys | all_flags;
	}

	u64 real_flags = 0;
	if (flags & PageFlags::Read) {
		real_flags |= FLAG_PRESENT;
	}
	if (flags & PageFlags::Write) {
		real_flags |= FLAG_RW;
	}
	if (!(flags & PageFlags::Execute)) {
		real_flags |= FLAG_NX;
	}
	if (flags & PageFlags::User) {
		real_flags |= FLAG_USER;
	}

	switch (cache_mode) {
		case CacheMode::WriteBack:
			break;
		case CacheMode::WriteCombine:
			real_flags |= FLAG_WT;
			break;
		case CacheMode::WriteThrough:
			real_flags |= FLAG_CD;
			break;
		case CacheMode::Uncached:
			real_flags |= FLAG_WT | FLAG_CD;
			break;
	}

	level3[level3_index] = phys | real_flags;

	return true;
}

void PageMap::unmap(u64 virt) {
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
		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (level2[level2_index] & FLAG_HUGE) {
			// todo 2mb huge page
			return;
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	level3[level3_index] = 0;
	orig_virt &= ~0xFFF;
	asm volatile("invlpg (%0)" : : "r"(orig_virt) : "memory");
}

u64 PageMap::get_phys(u64 virt) const {
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
		return 0;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return 0;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (level2[level2_index] & FLAG_HUGE) {
			return (level2[level2_index] & PAGE_ADDR_MASK) | (orig_virt & 0x1FFFFF);
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return 0;
	}

	return (level3[level3_index] & PAGE_ADDR_MASK) | (orig_virt & 0xFFF);
}

void PageMap::protect(u64 virt, PageFlags flags, CacheMode cache_mode) {
	u64 real_flags = 0;
	if (flags & PageFlags::Read) {
		real_flags |= FLAG_PRESENT;
	}
	if (flags & PageFlags::Write) {
		real_flags |= FLAG_RW;
	}
	if (!(flags & PageFlags::Execute)) {
		real_flags |= FLAG_NX;
	}
	if (flags & PageFlags::User) {
		real_flags |= FLAG_USER;
	}

	switch (cache_mode) {
		case CacheMode::WriteBack:
			break;
		case CacheMode::WriteCombine:
			real_flags |= FLAG_WT;
			break;
		case CacheMode::WriteThrough:
			real_flags |= FLAG_CD;
			break;
		case CacheMode::Uncached:
			real_flags |= FLAG_WT | FLAG_CD;
			break;
	}

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
		level2 = to_virt<u64>(level1[level1_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		if (level2[level2_index] & FLAG_HUGE) {
			// todo 2mb huge page
			return;
		}

		level3 = to_virt<u64>(level2[level2_index] & PAGE_ADDR_MASK);
	}
	else {
		return;
	}

	level3[level3_index] &= PAGE_ADDR_MASK;
	level3[level3_index] |= real_flags;

	orig_virt &= ~0xFFF;
	asm volatile("invlpg (%0)" : : "r"(orig_virt) : "memory");
}

void PageMap::use() {
	auto phys = to_phys(level0);
	asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
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

PageMap::~PageMap() {
	for (auto& page : used_pages) {
		pfree(page.phys(), 1);
	}
	used_pages.clear();
}

void PageMap::fill_high_half() {
	for (int i = 0; i < 512; ++i) {
		auto phys = pmalloc(1);
		assert(phys);
		memset(to_virt<void>(phys), 0, PAGE_SIZE);
		level0[i] = phys | FLAG_PRESENT | FLAG_RW;
		used_pages.push(Page::from_phys(phys));
	}
}

u64 PageMap::get_top_level_phys() const {
	return to_phys(level0);
}

ManuallyInit<PageMap> KERNEL_MAP;
