#pragma once
#include "types.hpp"
#include "rb_tree.hpp"
#include "vmem.hpp"
#include "compare.hpp"
#include "arch/paging.hpp"

class VirtualSpace {
public:
	void init(usize base, usize size);
	void destroy();

	void* alloc(usize size);
	void free(void* ptr, usize size);

	void* alloc_backed(usize size, PageFlags flags, CacheMode cache_mode = CacheMode::WriteBack);
	void free_backed(void* ptr, usize size);

private:
	struct Region {
		RbTreeHook hook {};
		usize base {};
		usize size {};

		constexpr int operator<=>(const Region& other) const {
			return kstd::threeway(base, other.base);
		}
	};

	VMem vmem;
	Spinlock<RbTree<Region, &Region::hook>> regions;
};

extern VirtualSpace KERNEL_VSPACE;
