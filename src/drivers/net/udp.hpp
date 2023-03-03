#pragma once
#include "nic.hpp"

struct UdpHeader {
	u16 src_port;
	u16 dst_port;
	u16 length;
	u16 checksum;
	u8 data[];

	void serialize();

	void update_checksum();
};

struct UdpPseudoIpv4Hdr {
	u32 src_addr;
	u32 dst_addr;
	u16 total_length;
	u8 protocol;
	u8 zero;
};

struct Packet;

void udp_process_packet(Nic* nic, Packet& packet, u8* data, u8 (&src)[6]);
void udp_create_hdr(Packet& packet, u16 src_port, u16 dst_port, u16 length);