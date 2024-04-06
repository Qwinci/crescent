#pragma once
#include "types.hpp"

enum class CacheMode {
	WriteCombine,
	WriteBack
};

constexpr usize PAGE_SIZE = 0x1000;

enum class PageFlags {
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	User = 1 << 3
};

constexpr PageFlags operator|(PageFlags lhs, PageFlags rhs) {
	return static_cast<PageFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

constexpr PageFlags& operator|=(PageFlags& lhs, PageFlags rhs) {
	lhs = lhs | rhs;
	return lhs;
}

struct PageMap {
	constexpr explicit PageMap(PageMap*) {}

	constexpr bool map(usize, usize, PageFlags, CacheMode) {
		return true;
	}

	constexpr usize get_phys(usize virt) {
		return virt;
	}

	constexpr void unmap(usize) {}

	constexpr void protect(usize, PageFlags, CacheMode) {}

	constexpr void use() {}
};
