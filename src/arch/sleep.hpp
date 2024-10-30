#pragma once

namespace acpi {
	extern usize SMP_TRAMPOLINE_PHYS_ADDR;
}

void arch_sleep_init();
void arch_prepare_for_sleep();
