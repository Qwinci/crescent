#include "udp.hpp"
#include "utils.hpp"

struct UdpHeader {
	u16 src_port;
	u16 dst_port;
	u16 length;
	u16 checksum;
	u8 data[];
};

void udp_process_packet(Nic* nic, u8* data, u8 (&src)[6]) {
	auto* hdr = cast<UdpHeader*>(data);
	// todo checksum

}