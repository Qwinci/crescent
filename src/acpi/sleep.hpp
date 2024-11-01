#pragma once

namespace acpi {
	enum class SleepState {
		S3,
		S5
	};

	void reboot();
	void enter_sleep_state(SleepState state);
	void wake_from_sleep();
}
