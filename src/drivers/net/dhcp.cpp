#include "dhcp.hpp"
#include "arp.hpp"
#include "console.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "memory/std.hpp"
#include "std/string.h"
#include "udp.hpp"
#include "utils/utils.hpp"

#define IP_UDP 0x11

enum class DhcpOp : u8 {
	BootRequest = 1,
	BootReply = 2
};

struct DhcpPacket {
	DhcpOp op;
	u8 htype;
	u8 hlen;
	u8 hops;
	u32 xid;
	u16 secs;
	u16 flags;
	u32 ciaddr;
	u32 yiaddr;
	u32 siaddr;
	u32 giaddr;
	u8 chaddr[16];
	u8 pad[192];
	u32 magic;
	u8 options[];

	void serialize() {
		xid = bswap32(xid);
		flags = bswap16(flags);
		secs = bswap16(secs);
		magic = bswap32(magic);
	}
};

#define DHCP_OPT_PAD 0
#define DHCP_OPT_REQ_IP_ADDR 50
#define DHCP_OPT_LEASE_TIME 51
#define DHCP_OPT_MSG_TYPE 53
#define DHCP_OPT_SERVER_ID 54
#define DHCP_OPT_PARAM_REQ_LIST 55
#define DHCP_OPT_CLIENT_ID 61
#define DHCP_OPT_END 0xFF

#define DHCP_DISCOVER 1
#define DHCP_OFFER 2
#define DHCP_REQUEST 3
#define DHCP_DECLINE 4
#define DHCP_ACK 5
#define DHCP_NAK 6
#define DHCP_RELEASE 7
#define DHCP_INFORM 8
#define DHCP_FORCE_RENEW 9
#define DHCP_LEASE_QUERY 10
#define DHCP_LEASE_UNASSIGNED 11
#define DHCP_LEASE_UNKNOWN 12
#define DHCP_LEASE_ACTIVE 13
#define DHCP_BULK_LEASE_QUERY 14
#define DHCP_LEASE_QUERY_DONE 15
#define DHCP_ACTIVE_LEASE_QUERY 16
#define DHCP_LEASE_QUERY_STATUS 17
#define DHCP_TLS 18

static Packet dhcp_create_packet(Nic* nic, const u8 (&dest)[6], u32 dst_ip, usize options_size) {
	Packet packet {};
	ethernet_create_hdr(nic, packet, ETH_IPV4, dest);
	u16 size = sizeof(UdpHeader) + sizeof(DhcpPacket) + options_size;
	ipv4_create_hdr(nic, packet, dest, size, IP_UDP, 0, dst_ip);
	packet.ipv4->serialize();
	packet.ipv4->update_checksum();

	udp_create_hdr(packet, 68, 67, sizeof(DhcpPacket) + options_size);
	packet.udp->serialize();

	packet.dhcp = cast<DhcpPacket*>(packet.end);
	packet.end += sizeof(DhcpPacket) + options_size;

	auto pseudo = cast<UdpPseudoIpv4Hdr*>(packet.end);
	*pseudo = {
		.src_addr = packet.ipv4->src_ip_addr,
		.dst_addr = packet.ipv4->dst_ip_addr,
		.total_length = packet.ipv4->total_length,
		.protocol = packet.ipv4->protocol,
		.zero = 0
	};

	auto dhcp = packet.dhcp;

	dhcp->op = DhcpOp::BootRequest;
	dhcp->htype = 1;
	dhcp->hlen = 6;
	dhcp->hops = 0;
	dhcp->xid = 0xCAFEBABE;
	dhcp->secs = 0;
	dhcp->ciaddr = 0;
	dhcp->yiaddr = 0;
	dhcp->siaddr = 0;
	dhcp->giaddr = 0;
	memcpy(dhcp->chaddr, nic->mac, sizeof(nic->mac));
	dhcp->magic = 0x63825363;

	return packet;
}

void dhcp_request(Nic* nic, u32 ip, u32 server_ip) {
	bool good = true;
	for (usize i = 0; i < 2; ++i) {
		arp_probe(nic, ip);
		// todo sched sleep
		udelay(US_IN_MS * 250);
		u8 mac[6];
		if (arp_query_ip(ip, mac)) {
			good = false;
			break;
		}
	}

	if (!good) {
		println("dhcp: address is in use");
		return;
	}

	auto packet = dhcp_create_packet(nic, NET_BROADCAST, 0xFFFFFFFF, 16);

	auto dhcp = packet.dhcp;

	dhcp->options[0] = DHCP_OPT_MSG_TYPE;
	dhcp->options[1] = 1;
	dhcp->options[2] = DHCP_REQUEST;

	dhcp->options[3] = DHCP_OPT_PAD;

	dhcp->options[4] = DHCP_OPT_REQ_IP_ADDR;
	dhcp->options[5] = 4;
	memcpy(&dhcp->options[6], &ip, sizeof(ip));

	dhcp->options[10] = DHCP_OPT_SERVER_ID;
	dhcp->options[11] = 4;
	memcpy(&dhcp->options[12], &server_ip, sizeof(server_ip));

	dhcp->serialize();

	nic->send(packet.begin, packet.size());
}

void dhcp_process_packet(Nic* nic, u8* data, const u8 (&src)[6]) {
	auto dhcp_hdr = cast<DhcpPacket*>(data);

	if (memcmp(dhcp_hdr->chaddr, nic->mac, sizeof(nic->mac)) != 0) {
		return;
	}

	if (dhcp_hdr->op != DhcpOp::BootReply) {
		return;
	}

	u32 server_ip = 0;
	bool offer = false;
	bool ack = false;

	for (u8* ptr = dhcp_hdr->options; *ptr != 0xFF;) {
		if (!*ptr) {
			++ptr;
			continue;
		}
		else if (*ptr == DHCP_OPT_MSG_TYPE) {
			ptr += 2;
			auto type = *ptr;

			if (type == DHCP_OFFER) {
				offer = true;
			}
			else if (type == DHCP_ACK) {
				ack = true;
			}

			ptr += 1;
		}
		else if (*ptr == DHCP_OPT_SERVER_ID) {
			ptr += 2;
			memcpy(&server_ip, ptr, sizeof(server_ip));
			ptr += 4;
		}
		else {
			println("unknown dhcp option: ", *ptr);
			u8 size = ptr[1];
			ptr += 2;
			ptr += size;
		}
	}

	if (nic->state == IpState::None && offer) {
		if (!server_ip) {
			println("dhcp: received offer without server identifier");
			return;
		}
		println("dhcp: received valid offer, requesting");
		dhcp_request(nic, dhcp_hdr->yiaddr, server_ip);
		nic->state = IpState::Requesting;
	}
	else if (ack) {
		println("dhcp: ack from server ",
				server_ip & 0xFF, ".",
				server_ip >> 8 & 0xFF, ".",
				server_ip >> 16 & 0xFF, ".",
				server_ip >> 24 & 0xFF);
		memcpy(nic->ipv4, &dhcp_hdr->yiaddr, sizeof(nic->ipv4));
		nic->state = IpState::Has;
	}
}

constexpr u16 FLAG_BROADCAST = 1 << 15;

void dhcp_discover(Nic* nic) {
	auto packet = dhcp_create_packet(nic, NET_BROADCAST, 0xFFFFFFFF, 5);
	auto dhcp_packet = packet.dhcp;

	dhcp_packet->flags = FLAG_BROADCAST;

	dhcp_packet->options[0] = DHCP_OPT_MSG_TYPE;
	// Length 1
	dhcp_packet->options[1] = 1;
	// Discover
	dhcp_packet->options[2] = DHCP_DISCOVER;

	// Pad
	dhcp_packet->options[3] = DHCP_OPT_PAD;

	// End
	dhcp_packet->options[4] = DHCP_OPT_END;

	dhcp_packet->serialize();

	nic->send(packet.begin, packet.size());
}