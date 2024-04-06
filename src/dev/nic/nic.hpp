#pragma once
#include "array.hpp"

struct Mac {
	kstd::array<u8, 6> data;
};

struct Nic {
	virtual ~Nic() = default;

	Mac mac {};
};
