#include "ip.hpp"

uint16_t net_checksum(const uint8_t* data, size_t count, ChecksumType type) {
	uint64_t sum = 0;

	while (count > 3) {
		sum += *(uint32_t*) data;
		count -= 4;
		data += 4;
	}

	while (count > 1) {
		sum += *(uint16_t*) data;
		count -= 2;
		data += 2;
	}

	if (count) {
		sum += *(uint8_t*) data;
	}

#define FOLD(sum) (((sum) & 0xFFFF) + ((sum) >> 16))

	if (type == ChecksumType::Ip) {
		sum = FOLD(FOLD(FOLD(FOLD(sum))));
	}
	else if (type == ChecksumType::Udp) {
		sum = FOLD(FOLD(FOLD(FOLD(~sum))));
	}

	return ~sum;
}