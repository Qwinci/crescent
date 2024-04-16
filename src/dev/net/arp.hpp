#pragma once
#include "types.hpp"
#include "bit.hpp"
#include "mac.hpp"

struct [[gnu::packed]] ArpHeader {
	u16 htype;
	u16 ptype;
	u8 hlen;
	u8 plen;
	u16 oper;
	Mac sender_hw_addr;
	u32 sender_protocol_addr;
	Mac target_hw_addr;
	u32 target_protocol_addr;

	inline void serialize() {
		htype = kstd::to_be(htype);
		ptype = kstd::to_be(ptype);
		oper = kstd::to_be(oper);
	}

	inline void deserialize() {
		htype = kstd::to_ne_from_be(htype);
		ptype = kstd::to_ne_from_be(ptype);
		oper = kstd::to_ne_from_be(oper);
	}
};

struct Nic;

void arp_process_packet(Nic& nic, void* data, const Mac& src);
