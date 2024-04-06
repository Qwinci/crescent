#include <cstring.hpp>
#include "early_paging.hpp"

constexpr usize BOOT_MEM_SIZE = 32 * PAGE_SIZE;
[[gnu::aligned(PAGE_SIZE)]] static u8 BOOT_MEM[BOOT_MEM_SIZE];
static usize BOOT_MEM_PTR = 0;

void* early_page_alloc() {
	if (BOOT_MEM_PTR == BOOT_MEM_SIZE) {
		__builtin_trap();
	}

	auto mem = BOOT_MEM + BOOT_MEM_PTR;
	BOOT_MEM_PTR += PAGE_SIZE;
	return mem;
}

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

void EarlyPageMap::map_1gb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	u64 real_flags = 0;
	u16 ap = 0;
	if (!(flags & PageFlags::Write)) {
		ap = AP_PRIVILEGED_R;
	}

	if (!(flags & PageFlags::Execute)) {
		real_flags |= FLAG_PXN;
	}

	virt >>= 30;
	u64 level1_index = virt & 0x1FF;
	virt >>= 9;
	u64 level0_index = virt & 0x1FF;

	u64* level1;
	if (level0[level0_index] & FLAG_PRESENT) {
		level1 = reinterpret_cast<u64*>(level0[level0_index] & PAGE_ADDR_MASK);
	} else {
		u64 page_phys = reinterpret_cast<u64>(early_page_alloc());
		level1 = reinterpret_cast<u64*>(page_phys);
		memset(level1, 0, PAGE_SIZE);
		level0[level0_index] = page_phys | TABLE_PRESENT;
	}

	u8 idx = static_cast<u8>(cache_mode);

	// outer shareable
	u16 sh = 0b10;

	level1[level1_index] = phys | FLAG_ACCESS | sh << 8 | ap << 6 | idx << 2 | FLAG_PRESENT | real_flags;
}

void EarlyPageMap::map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode) {
	u64 real_flags = 0;
	u16 ap = 0;
	if (!(flags & PageFlags::Write)) {
		ap = AP_PRIVILEGED_R;
	}

	if (!(flags & PageFlags::Execute)) {
		real_flags |= FLAG_PXN;
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
		level1 = reinterpret_cast<u64*>(level0[level0_index] & PAGE_ADDR_MASK);
	} else {
		u64 page_phys = reinterpret_cast<u64>(early_page_alloc());
		level1 = reinterpret_cast<u64*>(page_phys);
		memset(level1, 0, 0x1000);
		level0[level0_index] = page_phys | TABLE_PRESENT;
	}

	u64* level2;
	if (level1[level1_index] & FLAG_PRESENT) {
		level2 = reinterpret_cast<u64*>(level1[level1_index] & PAGE_ADDR_MASK);
	} else {
		u64 page_phys = reinterpret_cast<u64>(early_page_alloc());
		level2 = reinterpret_cast<u64*>(page_phys);
		memset(level2, 0, 0x1000);
		level1[level1_index] = page_phys | TABLE_PRESENT;
	}

	u64* level3;
	if (level2[level2_index] & FLAG_PRESENT) {
		level3 = reinterpret_cast<u64*>(level2[level2_index] & PAGE_ADDR_MASK);
	} else {
		u64 page_phys = reinterpret_cast<u64>(early_page_alloc());
		level3 = reinterpret_cast<u64*>(page_phys);
		memset(level3, 0, 0x1000);
		level2[level2_index] = page_phys | TABLE_PRESENT;
	}

	// outer shareable
	u16 sh = 0b10;

	u8 idx = static_cast<u8>(cache_mode);

	level3[level3_index] = phys | FLAG_ACCESS | sh << 8 | ap << 6 | idx << 2 | FLAG_4KB | FLAG_PRESENT | real_flags;
}
