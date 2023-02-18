#pragma once
#include "types.hpp"
#include "utils.hpp"

extern usize HHDM_OFFSET;

class PhysAddr;

class VirtAddr {
public:
	constexpr explicit VirtAddr(usize value) : value {value} {}

	template<typename T>
	inline explicit VirtAddr(T value) : value {cast<usize>(value)} {}

	[[nodiscard]] inline PhysAddr to_phys() const;

	[[nodiscard]] constexpr usize as_usize() const {
		return value;
	}

	[[nodiscard]] constexpr VirtAddr offset(isize offset) const {
		return VirtAddr {value + offset};
	}

	[[nodiscard]] constexpr VirtAddr offset(usize offset) const {
		return VirtAddr {value + offset};
	}

	constexpr operator void*() const { // NOLINT(google-explicit-constructor)
		return cast<void*>(value);
	}
private:
	usize value;
};

class PhysAddr {
public:
	constexpr explicit PhysAddr(usize value) : value {value} {}

	template<typename T>
	inline explicit PhysAddr(T value) : value {cast<usize>(value)} {}

	[[nodiscard]] inline VirtAddr to_virt() const {
		return VirtAddr {value + HHDM_OFFSET};
	}

	[[nodiscard]] constexpr usize as_usize() const {
		return value;
	}

	[[nodiscard]] constexpr PhysAddr offset(isize offset) const {
		return PhysAddr {value + offset};
	}

	[[nodiscard]] constexpr PhysAddr offset(usize offset) const {
		return PhysAddr {value + offset};
	}
private:
	usize value;
};

inline PhysAddr VirtAddr::to_phys() const {
	return PhysAddr {value - HHDM_OFFSET};
}

enum class PageFlags : u64 {
	Present = 1 << 0,
	Rw = 1 << 1,
	User = 1 << 2,
	WriteThrough = 1 << 3,
	CacheDisable = 1 << 4,
	Accessed = 1 << 5,
	Dirty = 1 << 6,
	Huge = 1 << 7,
	Global = 1ULL << 8,
	Nx = 1ULL << 63
};

constexpr PageFlags operator|(PageFlags lhs, PageFlags rhs) {
	return as<PageFlags>(as<u64>(lhs) | as<u64>(rhs));
}

constexpr bool operator&(PageFlags lhs, PageFlags rhs) {
	return as<u64>(lhs) & as<u64>(rhs);
}

constexpr void operator&=(PageFlags& lhs, PageFlags rhs) {
	lhs = as<PageFlags>(as<u64>(lhs) & as<u64>(rhs));
}

constexpr PageFlags operator~(PageFlags lhs) {
	return as<PageFlags>(~as<u64>(lhs));
}

constexpr usize SIZE_2MB = 0x200000;
constexpr usize SIZE_4GB = 0x100000000;

class PageMap {
public:
	void ensure_toplevel_entries();
	void map(VirtAddr virt, PhysAddr phys, PageFlags flags, bool split = false);
	void map_multiple(VirtAddr virt, PhysAddr phys, PageFlags flags, usize count, bool split = false);
	void unmap(VirtAddr virt, bool huge, bool dealloc);
	void load();
	void refresh_page(usize addr);
	void ensure_kernel_mapping(PhysAddr phys, usize size);
	PhysAddr virt_to_phys(VirtAddr addr);
	PageMap* create_high_half_shared();

	void destroy_lower_half();
private:
	struct Entry {
		u64 value {};

		[[nodiscard]] constexpr PhysAddr get_addr() const {
			return PhysAddr {value & 0x000FFFFFFFFFF000};
		}

		[[nodiscard]] constexpr PageFlags get_flags() const {
			return as<PageFlags>(value & 0xFFF0000000000FFF);
		}

		void set_addr(PhysAddr addr) {
			value &= 0xFFF0000000000FFF;
			value |= addr.as_usize();
		}

		void set_flags(PageFlags flags) {
			value &= 0x000FFFFFFFFFF000;
			value |= as<u64>(flags);
		}
	};
	static_assert(sizeof(Entry) == 8);
	Entry entries[512] {};
};

static inline PageMap* get_map() {
	extern PageMap* kernel_map;
	return kernel_map;
}