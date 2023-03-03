#include "udp.hpp"
#include "console.hpp"
#include "dhcp.hpp"
#include "ip.hpp"
#include "utils.hpp"
#include "utils/math.hpp"
#include "packet.hpp"

void UdpHeader::serialize() {
	src_port = bswap16(src_port);
	dst_port = bswap16(dst_port);
	length = bswap16(length);
	checksum = bswap16(checksum);
}

void UdpHeader::update_checksum() {
	checksum = net_checksum(cast<u8*>(this), as<usize>(bswap16(length)) + sizeof(UdpPseudoIpv4Hdr), ChecksumType::Udp);
}

void udp_process_packet(Nic* nic, Packet& packet, u8* data, u8 (&src)[6]) {
	auto* hdr = cast<UdpHeader*>(data);
	// todo checksum
	hdr->serialize();

	packet.third.udp = hdr;

	if (hdr->src_port == 67 && hdr->dst_port == 68) {
		println("received dhcp packet");
		dhcp_process_packet(nic, hdr->data, src);
	}
	else if (hdr->dst_port == 1234) {
		println("gdb packet");
		debug_process_packet(nic, packet, hdr->data, hdr->length - sizeof(UdpHeader), src);
	}
}

void udp_create_hdr(Packet& packet, u16 src_port, u16 dst_port, u16 length) {
	auto hdr = cast<UdpHeader*>(packet.end);
	packet.end += sizeof(UdpHeader);
	hdr->src_port = src_port;
	hdr->dst_port = dst_port;
	hdr->length = length + sizeof(UdpHeader);
	hdr->checksum = 0;

	packet.third.udp = hdr;
}
