#pragma once

namespace acpi {
	enum class SleepState {
		S5
	};

	void reboot();
	void enter_sleep_state(SleepState state);
}
