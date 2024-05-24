#pragma once
#include "types.hpp"
#include "ip.hpp"
#include "bit.hpp"

struct Ipv4Header {
	u8 ihl_version;
	u8 ecn_dscp;
	u16 total_len;
	u16 ident;
	u16 frag_flags;
	u8 ttl;
	IpProtocol protocol;
	u16 hdr_checksum;
	u32 src_addr;
	u32 dest_addr;

	void serialize() {
		total_len = kstd::to_be(total_len);
		ident = kstd::to_be(ident);
		frag_flags = kstd::to_be(frag_flags);
	}

	void deserialize() {
		total_len = kstd::to_ne_from_be(total_len);
		ident = kstd::to_ne_from_be(ident);
		frag_flags = kstd::to_ne_from_be(frag_flags);
	}

	void update_checksum();
};

struct Nic;
struct ReceivedPacket;

void ipv4_process_packet(Nic& nic, ReceivedPacket& packet);
