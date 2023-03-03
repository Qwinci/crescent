#include "console.hpp"
#include "ethernet.hpp"
#include "ip.hpp"
#include "udp.hpp"

void ipv4_process_packet(Nic* nic, Packet& packet, u8* data, u8 (&src)[6]) {
	auto ip_hdr = cast<Ipv4Header*>(data);
	ip_hdr->serialize();
	if (ip_hdr->get_version() != 4) {
		//println("IPv4 header has invalid version");
		return;
	}

	if (ip_hdr->flags_fragment_offset & 1 << 2) {
		println("ipv4: fragmentation is not supported");
		return;
	}

	packet.second.ipv4 = ip_hdr;

	data += ip_hdr->get_hdr_size();

	// UDP
	if (ip_hdr->protocol == 17) {
		println("UDP");
		udp_process_packet(nic, packet, data, src);
	}
	else {
		println("unknown IPv4 protocol: ", ip_hdr->protocol);
	}
}

void ipv4_create_hdr(Packet& packet, u16 size, u8 protocol, u32 src_ip, u32 dst_ip) {
	auto ip_hdr = cast<Ipv4Header*>(packet.end);
	packet.end += sizeof(Ipv4Header);
	ip_hdr->ihl_version = 5 | 4 << 4;
	ip_hdr->ecn_dscp = 0;
	ip_hdr->total_length = size + sizeof(Ipv4Header);
	ip_hdr->identification = 0;
	ip_hdr->flags_fragment_offset = 0;
	ip_hdr->time_to_live = 64;
	ip_hdr->protocol = protocol;
	ip_hdr->header_checksum = 0;
	ip_hdr->src_ip_addr = src_ip;
	ip_hdr->dst_ip_addr = dst_ip;
	packet.second.ipv4 = ip_hdr;
}