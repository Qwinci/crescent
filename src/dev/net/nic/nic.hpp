#pragma once
#include "dev/net/mac.hpp"
#include "manually_destroy.hpp"
#include "shared_ptr.hpp"
#include "vector.hpp"

struct Nic {
	constexpr Nic() {
		// 10.0.2.15/24
		ip = 0x0F'02'00'0A;
	}

	virtual ~Nic() = default;

	virtual void send(const void* data, u32 size) = 0;

	Mac mac {};
	u32 ip {};
};

extern ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Nic>>>> NICS;
