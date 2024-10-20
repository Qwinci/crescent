#pragma once
#include "types.hpp"
#include "optional.hpp"
#include "mem/pmalloc.hpp"
#include "manually_init.hpp"

constexpr usize PAGE_SIZE = 0x1000;

enum class PageFlags {
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	User = 1 << 3
};

enum class CacheMode {
	WriteBack,
	WriteCombine,
	WriteThrough,
	Uncached
};

constexpr PageFlags operator|(PageFlags lhs, PageFlags rhs) {
	return static_cast<PageFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
constexpr PageFlags& operator|=(PageFlags& lhs, PageFlags rhs) {
	lhs = lhs | rhs;
	return lhs;
}
constexpr bool operator&(PageFlags lhs, PageFlags rhs) {
	return static_cast<int>(lhs) & static_cast<int>(rhs);
}

class PageMap {
public:
	explicit PageMap(PageMap* kernel_map);
	~PageMap();

	[[nodiscard]] bool map_2mb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	[[nodiscard]] bool map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);

	void protect(u64 virt, PageFlags flags, CacheMode cache_mode);
	void protect_range(u64 virt, u64 size, PageFlags flags, CacheMode cache_mode);

	[[nodiscard]] u64 get_phys(u64 virt) const;

	void unmap(u64 virt);
	void use();

	constexpr bool operator==(const PageMap& other) const {
		return level0 == other.level0;
	}

	void fill_high_half();

private:
	u64* level0;
	DoubleList<Page, &Page::hook> used_pages {};
};
