#pragma once
#include <cstdint>

struct Ipv4 {
	uint32_t value;
};

struct Ipv6 {
	uint16_t value[8];
};
