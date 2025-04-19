#pragma once
#include "vbt_types.hpp"

namespace vbt {
	u32 get_block_size(const void* base);
	const void* get_block(const BdbHeader* hdr, BdbBlockId id);
}
