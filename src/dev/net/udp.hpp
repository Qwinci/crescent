#pragma once
#include "types.hpp"
#include "bit.hpp"

struct UdpHeader {
	u16 src_port;
	u16 dest_port;
	u16 length;
	u16 checksum;

	void serialize() {
		src_port = kstd::to_be(src_port);
		dest_port = kstd::to_be(dest_port);
		length = kstd::to_be(length);
	}

	void deserialize() {
		src_port = kstd::to_ne_from_be(src_port);
		dest_port = kstd::to_ne_from_be(dest_port);
		length = kstd::to_ne_from_be(length);
	}
};

struct Nic;
struct ReceivedPacket;

void udp_process_packet(Nic& nic, ReceivedPacket& packet);
