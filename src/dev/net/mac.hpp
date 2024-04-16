#pragma once
#include "array.hpp"
#include "types.hpp"

struct Mac {
	kstd::array<u8, 6> data;
};

static constexpr Mac BROADCAST_MAC {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
