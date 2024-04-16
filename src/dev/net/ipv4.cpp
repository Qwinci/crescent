#include "ipv4.hpp"
#include "udp.hpp"
#include "stdio.hpp"

void ipv4_process_packet(Nic& nic, void* data, const Mac& src) {
	auto* hdr = static_cast<Ipv4Header*>(data);
	hdr->deserialize();

	if ((hdr->frag_flags & 0x1FF) || (hdr->frag_flags >> 13 & 1U << 2)) {
		println("[kernel][ipv4]: fragmentation is not supported");
		return;
	}

	auto hdr_len = (hdr->ihl_version & 0xF) * 4;
	auto* payload = offset(hdr, void*, hdr_len);

	if (hdr->protocol == IpProtocol::Udp) {
		udp_process_packet(nic, payload, src);
	}
}

u16 ip_checksum(const u16* words, u16 count) {
	u32 res = 0;
	for (int i = 0; i < count; ++i) {
		res += words[i];
	}
	res = (res & 0xFFFF) | (res >> 16);
	res |= res >> 16;
	return ~res;
}

void Ipv4Header::update_checksum() {
	hdr_checksum = 0;
	auto* words = reinterpret_cast<const u16*>(this);
	hdr_checksum = ip_checksum(words, 20);
}
