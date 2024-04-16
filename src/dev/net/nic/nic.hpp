#pragma once
#include "dev/net/mac.hpp"

struct Nic {
	virtual ~Nic() = default;

	virtual void send(const void* data, u32 size) = 0;

	Mac mac {};
};
