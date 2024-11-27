#include "nic.hpp"
#include "dev/clock.hpp"

ManuallyDestroy<Spinlock<kstd::vector<kstd::shared_ptr<Nic>>>> NICS;

void Nic::wait_for_ip() {
	bool wait = false;
	{
		IrqGuard irq_guard {};
		auto guard = lock.lock();
		if (!ip) {
			wait = true;
		}
	}

	if (wait) {
		ip_available_event.wait();
	}

	assert(ip);
}

bool Nic::wait_for_ip_with_timeout(usize seconds) {
	bool wait = false;
	{
		IrqGuard irq_guard {};
		auto guard = lock.lock();
		if (!ip) {
			wait = true;
		}
	}

	if (wait) {
		if (!ip_available_event.wait_with_timeout(NS_IN_S * seconds)) {
			IrqGuard irq_guard {};
			auto guard = lock.lock();
			return ip;
		}
	}

	assert(ip);
	return true;
}

void nics_wait_ready() {
	IrqGuard irq_guard {};
	auto guard = NICS->lock();
	for (auto& nic : *guard) {
		nic->wait_for_ip_with_timeout(1);
	}
}
