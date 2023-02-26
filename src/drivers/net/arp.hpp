#pragma once
#include "types.hpp"

struct Nic;

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

	static ArpPacket* parse(u8* data);

	[[nodiscard]] ArpPacket serialize() const;
};

void arp_process_packet(Nic* nic, u8* data);
void arp_probe(Nic* nic, u32 ip);
bool arp_query_ip(u32 ip, u8 (&mac)[6]);