#include "console.hpp"
#include "ip.hpp"
#include "udp.hpp"

void ipv4_process_packet(Nic* nic, u8* data, u8 (&src)[6]) {
	auto ip_hdr = cast<Ipv4Header*>(data);
	if (ip_hdr->get_version() != 4) {
		//println("IPv4 header has invalid version");
		return;
	}

	data += ip_hdr->get_hdr_size();

	// UDP
	if (ip_hdr->protocol == 17) {
		println("UDP");
		udp_process_packet(nic, data, src);
	}
	else {
		println("unknown IPv4 protocol: ", ip_hdr->protocol);
	}
}