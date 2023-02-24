#pragma once

#include "types.hpp"

struct Nic {
	u8 mac[6] {};
	u8 ipv4[4] {};
	u8 ipv6[16] {};
	void (*send)(void* buffer, usize size) {};
	Nic* next {};
};

extern Nic* nics;
extern usize nic_count;

void register_nic(Nic* nic);