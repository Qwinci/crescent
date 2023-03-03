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
	explicit Packet(bool alloc = true);
	~Packet();

	u8* begin;
	u8* end;
	union First {
		EthernetHeader* ethernet;
	} first;
	union Second {
		Ipv4Header* ipv4;
		Ipv6Header* ipv6;
		ArpPacket* arp;
	} second;
	union {
		UdpHeader* udp;
		TcpHeader* tcp;
	} third;
	union {
		DhcpPacket* dhcp;
	} fourth;

	[[nodiscard]] usize size() const;
};
