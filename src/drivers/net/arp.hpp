#pragma once
#include "types.hpp"

struct Nic;

void arp_process_packet(Nic* nic, u8* data);