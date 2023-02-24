#pragma once
#include "nic.hpp"

void udp_process_packet(Nic* nic, u8* data, u8 (&src)[6]);