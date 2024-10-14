#include "sleep.hpp"
#include "acpi.hpp"
#include "manually_destroy.hpp"
#include "manually_init.hpp"
#include "qacpi/event.hpp"

namespace acpi {
	extern ManuallyDestroy<qacpi::events::Context> EVENT_CTX;

	void reboot() {
		auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S5);
		assert(status == qacpi::Status::Success);
		IrqGuard irq_guard {};
		EVENT_CTX->reboot();
#ifdef __x86_64__
		asm volatile("xor %%eax, %%eax; mov %%rax, %%cr3" : : : "memory");
#endif
	}

	void enter_sleep_state(SleepState state) {
		if (!GLOBAL_CTX.is_initialized()) {
			return;
		}

		switch (state) {
			case SleepState::S5:
			{
				auto status = EVENT_CTX->prepare_for_sleep_state(*GLOBAL_CTX, qacpi::events::SleepState::S5);
				assert(status == qacpi::Status::Success);
				IrqGuard shutdown_guard {};
				EVENT_CTX->enter_sleep_state(qacpi::events::SleepState::S5);
			}
		}
	}
}
