#include "ethernet.hpp"
#include "ipv4.hpp"
#include "arp.hpp"

void ethernet_process_packet(Nic& nic, void* data, usize) {
	auto* hdr = static_cast<EthernetHeader*>(data);
	hdr->deserialize();

	if (hdr->ether_type == EtherType::Ipv4) {
		ipv4_process_packet(nic, &hdr[1], hdr->src);
	}
	else if (hdr->ether_type == EtherType::Arp) {
		arp_process_packet(nic, &hdr[1], hdr->src);
	}
}
