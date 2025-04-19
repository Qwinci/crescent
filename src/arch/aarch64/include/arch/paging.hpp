#pragma once
#include "types.hpp"
#include "optional.hpp"
#include "manually_init.hpp"
#include "mem/pmalloc.hpp"

constexpr usize PAGE_SIZE = 0x1000;

enum class PageFlags {
	None,
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	User = 1 << 3
};

enum class CacheMode {
	WriteBack,
	WriteCombine,
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
	explicit PageMap(u64* early_map);
	~PageMap();

	[[nodiscard]] bool map_1gb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	[[nodiscard]] bool map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	void protect(u64 virt, PageFlags flags, CacheMode cache_mode);

	[[nodiscard]] u64 get_phys(u64 virt);

	void unmap(u64 virt);
	void use();

	void fill_high_half();

	constexpr bool operator==(const PageMap& other) const {
		return level0 == other.level0;
	}

	u64* level0;

private:
	DoubleList<Page, &Page::hook> used_pages {};
	Spinlock<void> lock {};
};
