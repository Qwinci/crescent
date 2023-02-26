#pragma once
#include "types.hpp"

struct EthernetHeader;
struct Ipv4Header;
struct Ipv6Header;
struct UdpHeader;
struct TcpHeader;
struct DhcpPacket;
struct ArpPacket;

struct Packet {
	Packet();
	~Packet();

	Packet(const Packet& other);

	u8* begin;
	u8* end;
	union {
		EthernetHeader* ethernet;
	};
	union {
		Ipv4Header* ipv4;
		Ipv6Header* ipv6;
		ArpPacket* arp;
	};
	union {
		UdpHeader* udp;
		TcpHeader* tcp;
	};
	union {
		DhcpPacket* dhcp;
	};

	[[nodiscard]] usize size() const;
};
