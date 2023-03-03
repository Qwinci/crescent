#pragma once
#include "packet.hpp"
#include "types.hpp"
#include "utils.hpp"
#include "utils/math.hpp"

constexpr u8 NET_BROADCAST[6] {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define ETH_IPV4 0x800
#define ETH_ARP 0x806
#define ETH_IPV6 0x86DD

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

struct Nic;

bool ethernet_process_packet(Nic* nic, u8* data, usize size);
void ethernet_create_hdr(Nic* nic, Packet& packet, u16 length, const u8 (&dest)[6]);