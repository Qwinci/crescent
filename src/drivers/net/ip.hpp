#pragma once
#include "types.hpp"

struct Ipv6Header {
private:
	u32 first;
public:
	u16 payload_length;
	u8 next_header;
	u8 hop_limit;
	u8 src_addr[4 * 4];
	u8 dst_addr[4 * 4];
	u8 payload[];

	static inline Ipv6Header* parse(u8* data) {
		auto* hdr = cast<Ipv6Header*>(data);
		hdr->first = bswap32(hdr->first);
		hdr->payload_length = bswap16(hdr->payload_length);
		return hdr;
	}

	static inline bool is_ipv6(u8 first) {
		return (first & 0b1111) == 6;
	}

	[[nodiscard]] inline u8 get_version() const {
		return first & 0b1111;
	}

	[[nodiscard]] inline u8 get_traffic_class() const {
		return first >> 4 & 0b11111111;
	}

	[[nodiscard]] inline u32 get_flow_label() const {
		return first >> 12;
	}
};
static_assert(sizeof(Ipv6Header) == 40);

struct Ipv4Header {
	u8 version_ihl;
	u8 dscp_ecn;
	u16 total_length;
	u16 identification;
	u16 flags_fragment_offset;
	u8 time_to_live;
	u8 protocol;
	u16 header_checksum;
	u32 src_ip_addr;
	u32 dst_ip_addr;

	[[nodiscard]] inline u8 get_version() const {
		return version_ihl & 0b1111;
	}

	[[nodiscard]] inline u8 get_hdr_size() const {
		return (version_ihl >> 4) * 4;
	}

	static inline bool is_ipv4(u8 first) {
		return (first & 0b1111) == 4;
	}

	static inline Ipv4Header* parse(u8* data) {
		auto* hdr = cast<Ipv4Header*>(data);
		hdr->total_length = bswap16(hdr->total_length);
		hdr->identification = bswap16(hdr->identification);
		hdr->flags_fragment_offset = bswap16(hdr->flags_fragment_offset);
		hdr->header_checksum = bswap16(hdr->header_checksum);
		hdr->src_ip_addr = bswap32(hdr->src_ip_addr);
		hdr->dst_ip_addr = bswap32(hdr->dst_ip_addr);
		return hdr;
	}
};
static_assert(sizeof(Ipv4Header) == 20);