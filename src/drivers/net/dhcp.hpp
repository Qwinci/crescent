#pragma once
#include "types.hpp"

struct Nic;

void dhcp_discover(Nic* nic);
void dhcp_process_packet(Nic* nic, u8* data, const u8 (&src)[6]);