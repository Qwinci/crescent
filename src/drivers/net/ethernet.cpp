#include "ethernet.hpp"
#include "arp.hpp"
#include "console.hpp"
#include "ip.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "memory/std.hpp"
#include "nic.hpp"

EthernetHeader* ethernet_create_packet(Nic* nic, u16 length, const u8 (&dest)[6]) {
	auto* hdr = as<EthernetHeader*>(as<void*>(PhysAddr {PAGE_ALLOCATOR.alloc_new()}.to_virt()));
	memcpy(hdr->dest_mac, dest, sizeof(dest));
	memcpy(hdr->src_mac, nic->mac, sizeof(nic->mac));
	hdr->length = length;
	return hdr;
}

bool ethernet_process_packet(Nic* nic, u8* data, usize size) {
	// todo dhcp

	auto ethernet_hdr = EthernetHeader::parse(data);
	if (ethernet_hdr->length <= 1500) {
		println("packet with length ", ethernet_hdr->length);
	}
	// IPv4
	else if (ethernet_hdr->length == 0x800) {
		println("IPv4");
		ipv4_process_packet(nic, ethernet_hdr->payload, ethernet_hdr->src_mac);
	}
	// ARP
	else if (ethernet_hdr->length == 0x806) {
		println("ARP");
		arp_process_packet(nic, ethernet_hdr->payload);
	}
	// IPv6
	else if (ethernet_hdr->length == 0x86DD) {
		println("IPv6");
	}
	else {
		println("ethernet_process_packet: unknown packet type ", Fmt::Hex, ethernet_hdr->length, Fmt::Dec);
	}

	return true;
}