#pragma once

namespace acpi {
	enum class SleepState {
		S5
	};

	void enter_sleep_state(SleepState state);
}
