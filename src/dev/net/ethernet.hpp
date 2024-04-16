#pragma once
#include "mac.hpp"
#include "bit.hpp"

struct Nic;

void ethernet_process_packet(Nic& nic, void* data, usize size);

enum class EtherType : u16 {
	Ipv4 = 0x800,
	Arp = 0x806,
	Ipv6 = 0x86DD
};

struct EthernetHeader {
	Mac dest;
	Mac src;
	EtherType ether_type;

	void serialize() {
		ether_type = static_cast<EtherType>(kstd::to_be(static_cast<u16>(ether_type)));
	}

	void deserialize() {
		ether_type = static_cast<EtherType>(kstd::to_ne_from_be(static_cast<u16>(ether_type)));
	}
};
