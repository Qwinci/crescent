#pragma once
#include "types.hpp"
#include "register.hpp"
#include "arch/paging.hpp"

class IoSpace : public MemSpace {
public:
	constexpr IoSpace() : MemSpace {0} {}
	inline explicit IoSpace(void* virt) : MemSpace {reinterpret_cast<usize>(virt)} {}
	constexpr IoSpace(usize phys, usize size) : MemSpace {0}, phys {phys}, size {size} {}

	constexpr void set_phys(usize new_phys, usize new_size) {
		phys = new_phys;
		size = new_size;
	}

	constexpr IoSpace subspace(usize offset) {
		return {*this, offset};
	}

	[[nodiscard]] bool map(CacheMode cache_mode);

	inline void* mapping() {
		return reinterpret_cast<void*>(base);
	}

private:
	constexpr IoSpace(const IoSpace& other, usize offset) : MemSpace {other.base + offset} {
		phys = other.phys + offset;
		size = other.size - offset;
	}

	usize phys {};
	usize size {};
};
