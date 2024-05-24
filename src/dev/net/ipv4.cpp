#include "ipv4.hpp"
#include "udp.hpp"
#include "tcp.hpp"
#include "stdio.hpp"
#include "packet.hpp"
#include "cstring.hpp"
#include "checksum.hpp"

void ipv4_process_packet(Nic& nic, ReceivedPacket& packet) {
	auto orig_layer1 = packet.layer1.raw;

	Ipv4Header hdr {};
	memcpy(&hdr, orig_layer1, sizeof(Ipv4Header));
	hdr.deserialize();

	packet.layer1.ipv4 = hdr;

	if ((hdr.frag_flags & 0x1FF) || (hdr.frag_flags >> 13 & 1U << 2)) {
		println("[kernel][ipv4]: fragmentation is not supported");
		return;
	}

	auto hdr_len = (hdr.ihl_version & 0xF) * 4;
	packet.layer2.raw = offset(orig_layer1, void*, hdr_len);
	packet.layer2_len = hdr.total_len - hdr_len;

	if (hdr.protocol == IpProtocol::Tcp) {
		tcp_process_packet(nic, packet);
	}
	else if (hdr.protocol == IpProtocol::Udp) {
		udp_process_packet(nic, packet);
	}
}

void Ipv4Header::update_checksum() {
	hdr_checksum = 0;
	Checksum sum;
	sum.add(this, 20);
	hdr_checksum = sum.get();
}
