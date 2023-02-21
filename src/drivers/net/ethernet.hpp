#pragma once
#include "types.hpp"
#include "utils.hpp"
#include "utils/math.hpp"

struct EthernetHeader {
	u8 dest_mac[6];
	u8 src_mac[6];
	u16 length; // or ethertype when larger than 1500
	u8 payload[];

	static inline EthernetHeader* parse(u8* data) {
		auto* hdr = cast<EthernetHeader*>(data);
		hdr->length = bswap16(hdr->length);
		return hdr;
	}
};