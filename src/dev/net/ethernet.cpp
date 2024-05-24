#include "ethernet.hpp"
#include "ipv4.hpp"
#include "arp.hpp"
#include "packet.hpp"

void ethernet_process_packet(Nic& nic, void* data, usize) {
	auto* hdr = static_cast<EthernetHeader*>(data);
	hdr->deserialize();

	ReceivedPacket packet {};
	packet.layer0.ethernet = *hdr;
	packet.layer1.raw = &hdr[1];

	if (hdr->ether_type == EtherType::Ipv4) {
		ipv4_process_packet(nic, packet);
	}
	else if (hdr->ether_type == EtherType::Arp) {
		arp_process_packet(nic, packet);
	}
}
