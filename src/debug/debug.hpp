#pragma once
#include "types.hpp"

enum class DebugState {
	None
};

struct Nic;
struct Packet;

void debug_process_packet(Nic* nic, Packet& in_packet, u8* data, u16 size, const u8 (&src)[6]);