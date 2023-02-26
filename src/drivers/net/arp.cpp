#include "arp.hpp"
#include "console.hpp"
#include "ethernet.hpp"
#include "memory/memory.hpp"
#include "nic.hpp"
#include "std/string.h"
#include "utils.hpp"
#include "utils/math.hpp"

class ArpHashTable {
public:
	void init() {
		if (!entries) {
			entries = new Entry[HASHTAB_SIZE]();
		}
	}

	struct Entry {
		u32 ip;
		u8 mac[6];
		u8 res[2];
	};

	bool insert(u32 ip, const u8 (&mac)[6]) {
		init();
		auto hash = murmur64(ip);
		usize index = hash % HASHTAB_SIZE;
		auto start = index;

		auto& entry = entries[start];

		if (entry.ip) {
			return false;
		}

		entry.ip = ip;
		memcpy(entry.mac, mac, sizeof(mac));
		return true;
	}

	Entry& get(u32 ip) {
		init();
		auto hash = murmur64(ip);
		return entries[hash % HASHTAB_SIZE];
	}

	void remove(u32 ip) {
		init();
		auto hash = murmur64(ip);
		auto entry = entries[hash % HASHTAB_SIZE];
		entry.ip = 0;
	}

	void clear() {
		init();
		for (usize i = 0; i < HASHTAB_SIZE; ++i) {
			auto& entry = entries[i];
			entry.ip = 0;
		}
	}
private:
	static constexpr usize HASHTAB_SIZE = PAGE_SIZE / sizeof(Entry);
	Entry* entries;
};

static ArpHashTable arp_hashtab {};

bool arp_query_ip(u32 ip, u8 (&mac)[6]) {
	auto flags = enter_critical();
	const auto& entry = arp_hashtab.get(ip);
	if (entry.ip) {
		memcpy(mac, entry.mac, sizeof(entry.mac));
		leave_critical(flags);
		return true;
	}
	leave_critical(flags);
	return false;
}

void arp_probe(Nic* nic, u32 ip) {
	Packet packet {};
	ethernet_create_hdr(nic, packet, 0x806, NET_BROADCAST);

	ArpPacket probe {
		.hardware_type = 1,
		.protocol_type = 0x800,
		.hardware_addr_len = 6,
		.protocol_addr_len = 4,
		.operation = ArpOp::Request,
	};
	memcpy(probe.sender_hw_addr, nic->mac, sizeof(nic->mac));
	memcpy(probe.target_protocol_addr, &ip, sizeof(ip));

	probe = probe.serialize();

	packet.arp = cast<ArpPacket*>(packet.end);
	packet.end += sizeof(ArpPacket);

	memcpy(packet.arp, &probe, sizeof(probe));

	nic->send(packet.begin, packet.size());
}

void arp_process_packet(Nic* nic, u8* data) {
	auto arp_packet = ArpPacket::parse(data);
	// todo
	nic->ipv4[0] = 192;
	nic->ipv4[1] = 168;
	nic->ipv4[2] = 1;
	nic->ipv4[3] = 106;

	if (arp_packet->operation == ArpOp::Reply) {
		u32 ip = arp_packet->sender_protocol_addr[0];
		ip |= as<u32>(arp_packet->sender_protocol_addr[1]) << 8;
		ip |= as<u32>(arp_packet->sender_protocol_addr[2]) << 16;
		ip |= as<u32>(arp_packet->sender_protocol_addr[3]) << 24;
		arp_hashtab.insert(ip, arp_packet->sender_hw_addr);
	}

	if (memcmp(arp_packet->target_protocol_addr, nic->ipv4, sizeof(nic->ipv4)) == 0) {
		//println("arp packet IP match");
		if (arp_packet->operation == ArpOp::Request) {
			Packet packet {};
			ethernet_create_hdr(nic, packet, 0x806, NET_BROADCAST);

			ArpPacket reply {
				.hardware_type = 1,
				.protocol_type = 0x800,
				.hardware_addr_len = 6,
				.protocol_addr_len = 4,
				.operation = ArpOp::Reply,
			};
			memcpy(reply.sender_hw_addr, nic->mac, sizeof(nic->mac));
			memcpy(reply.sender_protocol_addr, arp_packet->target_protocol_addr, sizeof(reply.sender_protocol_addr));
			memcpy(reply.target_hw_addr, arp_packet->sender_hw_addr, sizeof(reply.target_hw_addr));
			memcpy(reply.target_protocol_addr, arp_packet->sender_protocol_addr, sizeof(reply.target_protocol_addr));

			reply = reply.serialize();

			packet.arp = cast<ArpPacket*>(packet.end);
			packet.end += sizeof(ArpPacket);

			memcpy(packet.arp, &reply, sizeof(reply));

			nic->send(packet.begin, packet.size());
			println("ARP reply sent");
		}
		else {
			println("ARP response");
		}
	}
}

ArpPacket* ArpPacket::parse(u8* data) {
	auto* packet = cast<ArpPacket*>(data);
	packet->hardware_type = bswap16(packet->hardware_type);
	packet->protocol_type = bswap16(packet->protocol_type);
	packet->operation = as<ArpOp>(bswap16(as<u16>(packet->operation)));
	return packet;
}

ArpPacket ArpPacket::serialize() const {
	ArpPacket packet = *this;
	packet.hardware_type = bswap16(hardware_type);
	packet.protocol_type = bswap16(protocol_type);
	packet.operation = as<ArpOp>(bswap16(as<u16>(operation)));
	return packet;
}
