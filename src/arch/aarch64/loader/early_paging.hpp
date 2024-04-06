#pragma once
#include "types.hpp"
#include "optional.hpp"
#include "arch/paging.hpp"

class EarlyPageMap {
public:
	void map_1gb(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);
	void map(u64 virt, u64 phys, PageFlags flags, CacheMode cache_mode);

	u64 level0[512];
};

void* early_page_alloc();
