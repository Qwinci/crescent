#include "packet.hpp"
#include "mem/malloc.hpp"
#include "assert.hpp"
#include "new.hpp"
#include "cstring.hpp"

Packet::Packet(u32 size) : size {size} {
	data = ALLOCATOR.alloc(size);
	assert(data);
}

void Packet::add_ethernet(const Mac& src, const Mac& dest, EtherType ether_type) {
	ethernet = new (add_header(sizeof(EthernetHeader))) EthernetHeader {
		.dest = dest,
		.src = src,
		.ether_type = ether_type
	};
	ethernet->serialize();
}

void Packet::add_ipv4(IpProtocol protocol, u16 payload_size, u32 src_addr, u32 dest_addr) {
	Ipv4Header hdr {
		.ihl_version = 5 | 4 << 4,
		.ecn_dscp = 0,
		.total_len = static_cast<u16>(20 + payload_size),
		.ident = 0,
		.frag_flags = 0,
		.ttl = 64,
		.protocol = protocol,
		.hdr_checksum = 0,
		.src_addr = src_addr,
		.dest_addr = dest_addr
	};
	hdr.serialize();
	hdr.update_checksum();

	ipv4 = static_cast<Ipv4Header*>(add_header(sizeof(Ipv4Header)));
	memcpy(ipv4, &hdr, sizeof(hdr));
}

void Packet::add_udp(u16 src_port, u16 dest_port, u16 length) {
	udp = new (add_header(sizeof(UdpHeader))) UdpHeader {
		.src_port = src_port,
		.dest_port = dest_port,
		.length = static_cast<u16>(8 + length),
		.checksum = 0
	};
	udp->serialize();
}

void* Packet::add_header(u32 hdr_size) {
	auto* ptr = offset(data, void*, offset);
	offset += hdr_size;
	return ptr;
}

Packet::~Packet() {
	ALLOCATOR.free(data, size);
}
