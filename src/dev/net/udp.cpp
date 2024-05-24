#include "udp.hpp"
#include "nic/nic.hpp"
#include "packet.hpp"
#include "cstring.hpp"
#include "stdio.hpp"

void gdb_process_packet(Nic& nic, void* data, u16 size, const Mac& src) {
	const char* str = static_cast<const char*>(data);

	println("gdb packet received: ", kstd::string_view {str, size});

	u16 reply_size = 5;
	char reply_data[] = "+$#00";

	Packet packet {static_cast<u32>(sizeof(EthernetHeader) + sizeof(Ipv4Header) + sizeof(UdpHeader) + reply_size)};
	packet.add_ethernet(nic.mac, src, EtherType::Ipv4);
	packet.add_ipv4(
		IpProtocol::Udp,
		sizeof(UdpHeader) + reply_size,
		192 | 168 << 8 | 1 << 16 | 106 << 24,
		192 | 168 << 8 | 1 << 16 | 102 << 24);
	packet.add_udp(1234, 1234, reply_size);
	auto* ptr = packet.add_header(reply_size);
	memcpy(ptr, reply_data, reply_size);

	nic.send(packet.data, packet.size);
}

void udp_process_packet(Nic& nic, ReceivedPacket& packet) {
	auto* hdr = static_cast<UdpHeader*>(packet.layer2.raw);
	hdr->deserialize();

	packet.layer2.udp = *hdr;

	if (hdr->dest_port == 1234) {
		gdb_process_packet(nic, &hdr[1], hdr->length - 8, packet.layer0.ethernet.src);
	}
}
