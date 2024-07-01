#pragma once
#include "dev/event.hpp"
#include "dev/net/mac.hpp"
#include "manually_destroy.hpp"
#include "shared_ptr.hpp"
#include "vector.hpp"

struct Nic {
	virtual ~Nic() = default;

	virtual void send(const void* data, u32 size) = 0;

	void wait_for_ip();
	bool wait_for_ip_with_timeout(usize seconds);

	Mac mac {};
	u32 ip {};
	Spinlock<void> lock {};
	Event ip_available_event {};
};

void nics_wait_ready();

extern ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Nic>>>> NICS;
