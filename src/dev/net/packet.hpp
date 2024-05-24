#pragma once
#include "ethernet.hpp"
#include "ipv4.hpp"
#include "udp.hpp"

struct Packet {
	explicit Packet(u32 size);
	~Packet();

	void* add_header(u32 hdr_size);

	void add_ethernet(const Mac& src, const Mac& dest, EtherType ether_type);
	void add_ipv4(IpProtocol protocol, u16 payload_size, u32 src_addr, u32 dest_addr);
	void add_udp(u16 src_port, u16 dest_port, u16 length);

	union {
		EthernetHeader* ethernet {};
	};
	union {
		Ipv4Header* ipv4 {};
	};
	union {
		UdpHeader* udp {};
	};

	void* data;
	u32 size;
	u32 offset {};
};

struct ReceivedPacket {
	union {
		EthernetHeader ethernet;
	} layer0;
	union {
		void* raw;
		Ipv4Header ipv4;
	} layer1;
	union {
		void* raw;
		UdpHeader udp;
	} layer2;
	u16 layer2_len;
};
