#include "arp.hpp"
#include "nic/nic.hpp"
#include "packet.hpp"
#include "stdio.hpp"
#include "new.hpp"

void arp_process_packet(Nic& nic, void* data, const Mac& src) {
	auto* hdr = static_cast<ArpHeader*>(data);
	hdr->deserialize();
	if (hdr->oper != 1) {
		return;
	}

	if (hdr->htype != 1 || hdr->ptype != 0x800 ||
		hdr->hlen != 6 || hdr->plen != 4) {
		println("[kernel][arp]: unsupported request");
		return;
	}

	if (hdr->target_protocol_addr == (192 | 168 << 8 | 1 << 16 | 106 << 24)) {
		Packet packet {sizeof(EthernetHeader) + sizeof(ArpHeader)};
		packet.add_ethernet(nic.mac, BROADCAST_MAC, EtherType::Arp);
		auto* reply_hdr = new (packet.add_header(sizeof(ArpHeader))) ArpHeader {
			.htype = 1,
			.ptype = 0x800,
			.hlen = 6,
			.plen = 4,
			.oper = 2,
			.sender_hw_addr = nic.mac,
			.sender_protocol_addr = hdr->target_protocol_addr,
			.target_hw_addr = hdr->sender_hw_addr,
			.target_protocol_addr = hdr->sender_protocol_addr
		};
		reply_hdr->serialize();
		nic.send(packet.data, packet.size);
	}
}
