#pragma once
#include "types.hpp"

struct I2cMessage {
	constexpr I2cMessage(u16 addr, u16 len, bool read, u8* data)
		: addr {addr}, len {len}, read {read}, data {data} {}

	u16 addr;
	u16 len;
	bool read;
	u8* data;
};
