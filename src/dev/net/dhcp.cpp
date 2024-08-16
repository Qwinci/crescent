#include "dhcp.hpp"
#include "nic/nic.hpp"
#include "packet.hpp"
#include "udp.hpp"

enum class Option : u8 {
	Pad = 0,
	SubnetMask = 1,
	Router = 3,
	DnsServer = 6,
	RequestedIp = 50,
	LeaseTime = 51,
	MessageType = 53,
	DhcpServer = 54,
	ParameterRequestList = 55,
	End = 0xFF
};

enum class Op : u8 {
	Discover = 1,
	Offer = 2,
	Request = 3,
	Decline = 4,
	Ack = 5,
	Nak = 6,
	Release = 7
};

struct DhcpHeader {
	u8 op {};
	u8 htype {};
	u8 hlen {};
	u8 hops {};
	u32 xid {};
	u16 secs {};
	u16 flags {};
	u32 ciaddr {};
	u32 yiaddr {};
	u32 siaddr {};
	u32 giaddr {};
	u8 chaddr[16] {};
	u8 zero[192] {};
	u32 magic_cookie {kstd::to_be(0x63825363)};

	void serialize() {
		xid = kstd::to_be(xid);
		secs = kstd::to_be(secs);
		flags = kstd::to_be(flags);
	}

	void deserialize() {
		xid = kstd::to_ne_from_be(xid);
		secs = kstd::to_ne_from_be(secs);
		flags = kstd::to_ne_from_be(flags);
	}
};

void dhcp_process_packet(Nic& nic, ReceivedPacket& packet) {
	UdpHeader udp_hdr {};
	memcpy(&udp_hdr, packet.layer2.raw, sizeof(UdpHeader));
	udp_hdr.deserialize();

	kstd::vector<u8> tmp;
	tmp.resize(udp_hdr.length - sizeof(UdpHeader));
	memcpy(tmp.data(), offset(packet.layer2.raw, void*, sizeof(UdpHeader)), udp_hdr.length - sizeof(UdpHeader));

	auto* dhcp_hdr = reinterpret_cast<DhcpHeader*>(tmp.data());
	dhcp_hdr->deserialize();
	auto* options = reinterpret_cast<u8*>(&dhcp_hdr[1]);
	u16 options_len = tmp.size() - sizeof(DhcpHeader);

	if (memcmp(dhcp_hdr->chaddr, nic.mac.data.data, 6) == 0) {
		u32 ip = dhcp_hdr->yiaddr;
		u32 lease_seconds = 0;
		u32 dns_server = 0;
		u32 subnet_mask = 0;
		Op type;

		for (int i = 0; i < options_len;) {
			auto op = static_cast<Option>(options[i]);
			if (op == Option::Pad) {
				++i;
				continue;
			}
			else if (op == Option::SubnetMask) {
				memcpy(&subnet_mask, options + i + 2, 4);
			}
			else if (op == Option::DnsServer) {
				memcpy(&dns_server, options + i + 2, 4);
			}
			else if (op == Option::LeaseTime) {
				memcpy(&lease_seconds, options + i + 2, 4);
				lease_seconds = kstd::to_ne_from_be(lease_seconds);
			}
			else if (op == Option::MessageType) {
				type = static_cast<Op>(options[i + 2]);
			}

			u8 len = options[i + 1];
			i += 2 + len;
		}

		if (type == Op::Offer) {
			// todo check if ip is in use with arp
			options_len = 16;
			u16 request_payload_size = sizeof(UdpHeader) + sizeof(DhcpHeader) + options_len;

			Packet request_packet {static_cast<u32>(sizeof(EthernetHeader) + sizeof(Ipv4Header) + request_payload_size)};
			request_packet.add_ethernet(nic.mac, BROADCAST_MAC, EtherType::Ipv4);
			request_packet.add_ipv4(IpProtocol::Udp, request_payload_size, 0, 0xFFFFFFFF);
			request_packet.add_udp(68, 67, request_payload_size - sizeof(UdpHeader));

			auto* ptr = request_packet.add_header(request_payload_size - sizeof(UdpHeader));
			DhcpHeader discover_hdr {
				.op = 1,
				.htype = 1,
				.hlen = 6,
				.xid = 0x3903F326,
				.siaddr = dhcp_hdr->siaddr
			};
			memcpy(discover_hdr.chaddr, nic.mac.data.data, 6);
			discover_hdr.serialize();
			memcpy(ptr, &discover_hdr, sizeof(discover_hdr));
			options = offset(ptr, u8*, sizeof(DhcpHeader));

			// dhcp message type request
			options[0] = static_cast<u8>(Option::MessageType);
			// len
			options[1] = 1;
			options[2] = static_cast<u8>(Op::Request);

			// requested ip
			options[3] = static_cast<u8>(Option::RequestedIp);
			// len
			options[4] = 4;
			memcpy(options + 5, &ip, 4);

			// dhcp server
			options[9] = static_cast<u8>(Option::DhcpServer);
			// len
			options[10] = 4;
			memcpy(options + 11, &dhcp_hdr->siaddr, 4);

			// end
			options[15] = static_cast<u8>(Option::End);

			nic.send(request_packet.data, request_packet.size);
		}
		else if (type == Op::Ack) {
			u8 first = ip;
			u8 second = ip >> 8;
			u8 third = ip >> 16;
			u8 fourth = ip >> 24;

			println(
				"ip ", first, ".", second, ".", third, ".", fourth,
				" leased for ", lease_seconds, " seconds");

			IrqGuard irq_guard {};
			auto guard = nic.lock.lock();
			nic.ip = ip;
			nic.subnet_mask = subnet_mask;
			nic.gateway_ip = dhcp_hdr->giaddr;
			if (!nic.gateway_ip) {
				nic.gateway_ip = dhcp_hdr->siaddr;
			}
			nic.ip_available_event.signal_count(256);
		}
	}
}

void dhcp_discover(Nic* nic) {
	u16 options_len = 9;
	u16 discover_payload_size = sizeof(UdpHeader) + sizeof(DhcpHeader) + options_len;

	Packet discover_packet {static_cast<u32>(sizeof(EthernetHeader) + sizeof(Ipv4Header) + discover_payload_size)};
	discover_packet.add_ethernet(nic->mac, BROADCAST_MAC, EtherType::Ipv4);
	discover_packet.add_ipv4(IpProtocol::Udp, discover_payload_size, 0, 0xFFFFFFFF);
	discover_packet.add_udp(68, 67, discover_payload_size - sizeof(UdpHeader));

	auto* ptr = discover_packet.add_header(discover_payload_size - sizeof(UdpHeader));
	DhcpHeader discover_hdr {
		.op = 1,
		.htype = 1,
		.hlen = 6,
		.xid = 0x3903F326
	};
	memcpy(discover_hdr.chaddr, nic->mac.data.data, 6);
	discover_hdr.serialize();
	memcpy(ptr, &discover_hdr, sizeof(discover_hdr));
	auto* options = offset(ptr, u8*, sizeof(DhcpHeader));

	// dhcp message type discover
	options[0] = static_cast<u8>(Option::MessageType);
	// len
	options[1] = 1;
	options[2] = static_cast<u8>(Op::Discover);

	// parameter request list
	options[3] = static_cast<u8>(Option::ParameterRequestList);
	// len
	options[4] = 1;
	// domain name server
	options[5] = 6;
	// len
	options[6] = 4;
	// subnet mask
	options[7] = 1;

	// end
	options[8] = static_cast<u8>(Option::End);

	nic->send(discover_packet.data, discover_packet.size);
}
