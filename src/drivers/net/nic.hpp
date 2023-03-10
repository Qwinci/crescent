#pragma once
#include "types.hpp"

enum class IpState : u8 {
	None,
	Requesting,
	Has
};

struct Nic {
	u8 mac[6] {};
	union {
		u8 arr[4] {};
		u32 whole;
	} ipv4;
	u8 ipv6[16] {};
	void (*send)(void* buffer, usize size) {};

	Nic* next {};
	IpState state {};
};

extern Nic* nics;
extern usize nic_count;

void register_nic(Nic* nic);