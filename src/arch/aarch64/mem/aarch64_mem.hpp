#pragma once
#include "types.hpp"

struct MemReserve {
	usize base;
	usize len;
};

void aarch64_mem_check_add_usable_range(usize base, usize len, MemReserve* reserve, u32 reserve_count);
