#include "arp.hpp"
#include "console.hpp"
#include "ethernet.hpp"
#include "nic.hpp"
#include "std/string.h"
#include "utils.hpp"
#include "utils/math.hpp"

enum class ArpOp : u16 {
	Request = 1,
	Reply = 2,
	RequestReverse = 3,
	ReplyReverse = 4,
	DrarpRequest = 5,
	DrarpReply = 6,
	DrarpError = 7,
	InArpRequest = 8,
	InArpReply = 9,
	ArpNak = 10,
	MarsRequest = 11,
	MarsMulti = 12,
	MarsMServ = 13,
	MarsJoin = 14,
	MarsLeave = 15,
	MarsNak = 16,
	MarsUnServ = 17,
	MarsSJoin = 18,
	MarsSLeave = 19,
	MarsGroupListRequest = 20,
	MarsGroupListReply = 21,
	MarsRedirectMap = 22,
	MapOsUnArp = 23
};

struct ArpPacket {
	u16 hardware_type;
	u16 protocol_type;
	u8 hardware_addr_len;
	u8 protocol_addr_len;
	ArpOp operation;
	u8 sender_hw_addr[6];
	u8 sender_protocol_addr[4];
	u8 target_hw_addr[6];
	u8 target_protocol_addr[4];

	static ArpPacket* parse(u8* data) {
		auto* packet = cast<ArpPacket*>(data);
		packet->hardware_type = bswap16(packet->hardware_type);
		packet->protocol_type = bswap16(packet->protocol_type);
		packet->operation = as<ArpOp>(bswap16(as<u16>(packet->operation)));
		return packet;
	}

	[[nodiscard]] ArpPacket serialize() const {
		ArpPacket packet = *this;
		packet.hardware_type = bswap16(hardware_type);
		packet.protocol_type = bswap16(protocol_type);
		packet.operation = as<ArpOp>(bswap16(as<u16>(operation)));
		return packet;
	}
};

void arp_process_packet(Nic* nic, u8* data) {
	auto arp_packet = ArpPacket::parse(data);
	// todo
	nic->ipv4[0] = 192;
	nic->ipv4[1] = 168;
	nic->ipv4[2] = 1;
	nic->ipv4[3] = 106;
	/*println("ARP sender ip: ",
			arp_packet->sender_protocol_addr[0], ".",
			arp_packet->sender_protocol_addr[1], ".",
			arp_packet->sender_protocol_addr[2], ".",
			arp_packet->sender_protocol_addr[3],
			" target ip: ",
			arp_packet->target_protocol_addr[0], ".",
			arp_packet->target_protocol_addr[1], ".",
			arp_packet->target_protocol_addr[2], ".",
			arp_packet->target_protocol_addr[3]);*/

	if (memcmp(arp_packet->target_protocol_addr, nic->ipv4, sizeof(nic->ipv4)) == 0) {
		//println("arp packet IP match");
		if (arp_packet->operation == ArpOp::Request) {
			const u8 broadcast[6] {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
			auto ethernet_packet = ethernet_create_packet(nic, 0x806, broadcast);

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

			memcpy(ethernet_packet->payload, &reply, sizeof(reply));

			nic->send(ethernet_packet, sizeof(*ethernet_packet) + sizeof(reply));
			println("ARP reply sent");
		}
		else {
			println("ARP response");
		}
	}
}