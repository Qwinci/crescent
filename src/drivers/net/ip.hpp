#pragma once
#include "types.hpp"

struct Ipv4Header {
	u8 version : 4;
	u8 ihl : 4;
	u8 dscp : 6;
	u8 ecn : 2;
	u16 total_length;
	u16 identification;
	u16 flags : 3;
	u16 fragment_offset : 13;
	u8 time_to_live;
	u8 protocol;
	u16 header_checksum;
	u32 src_ip_addr;
	u32 dst_ip_addr;
};
static_assert(sizeof(Ipv4Header) == 20);