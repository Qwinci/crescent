#include "arp.hpp"
#include "dev/clock.hpp"
#include "dev/event.hpp"
#include "manually_destroy.hpp"
#include "new.hpp"
#include "nic/nic.hpp"
#include "packet.hpp"
#include "stdio.hpp"
#include "unordered_map.hpp"

namespace {
	ManuallyDestroy<kstd::unordered_map<Mac, u32>> IP_TABLE;
	Event new_ip_event {};
}

void arp_send_query(Nic& nic, u32 ip) {
	Packet packet {sizeof(EthernetHeader) + sizeof(ArpHeader)};
	packet.add_ethernet(nic.mac, BROADCAST_MAC, EtherType::Arp);

	ArpHeader hdr {
		.htype = 1,
		.ptype = 0x800,
		.hlen = 6,
		.plen = 4,
		.oper = 1,
		.sender_hw_addr = nic.mac,
		.sender_protocol_addr = nic.ip,
		.target_hw_addr {},
		.target_protocol_addr = ip
	};
	hdr.serialize();

	auto* hdr_ptr = packet.add_header(sizeof(ArpHeader));
	memcpy(hdr_ptr, &hdr, sizeof(hdr));
	nic.send(packet.data, packet.size);
}

kstd::optional<Mac> arp_get_mac(u32 ip) {
	bool found = false;
	{
		IrqGuard irq_guard {};
		auto guard = NICS->lock();
		for (auto& nic : *guard) {
			if (nic->ip && (nic->ip & nic->subnet_mask) == (ip & nic->subnet_mask)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		// todo choose nic
		IrqGuard irq_guard {};
		auto guard = NICS->lock();
		if (guard->is_empty()) {
			return {};
		}
		ip = (*guard->front())->gateway_ip;
	}

	while (true) {
		auto ptr = IP_TABLE->get(ip);
		if (ptr) {
			return *ptr;
		}

		{
			IrqGuard irq_guard {};
			auto guard = NICS->lock();

			// todo choose nic
			arp_send_query(**guard->front(), ip);
		}

		bool status = new_ip_event.wait_with_timeout(US_IN_S * 2);
		if (!status) {
			return {};
		}
	}
}

void arp_process_packet(Nic& nic, ReceivedPacket& packet) {
	auto* hdr = static_cast<ArpHeader*>(packet.layer1.raw);
	hdr->deserialize();

	if (hdr->oper == 1 || hdr->oper == 2) {
		IP_TABLE->insert(hdr->sender_protocol_addr, packet.layer0.ethernet.src);
		new_ip_event.signal_all();
	}

	if (hdr->oper != 1) {
		return;
	}

	if (hdr->htype != 1 || hdr->ptype != 0x800 ||
		hdr->hlen != 6 || hdr->plen != 4) {
		println("[kernel][arp]: unsupported request");
		return;
	}

	if (hdr->target_protocol_addr == nic.ip) {
		Packet new_packet {sizeof(EthernetHeader) + sizeof(ArpHeader)};
		new_packet.add_ethernet(nic.mac, BROADCAST_MAC, EtherType::Arp);
		auto* reply_hdr = new (new_packet.add_header(sizeof(ArpHeader))) ArpHeader {
			.htype = 1,
			.ptype = 0x800,
			.hlen = 6,
			.plen = 4,
			.oper = 2,
			.sender_hw_addr = nic.mac,
			.sender_protocol_addr = nic.ip,
			.target_hw_addr = hdr->sender_hw_addr,
			.target_protocol_addr = hdr->sender_protocol_addr
		};
		reply_hdr->serialize();
		nic.send(new_packet.data, new_packet.size);
	}
}
