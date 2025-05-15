#pragma once

namespace acpi {
	extern usize SMP_TRAMPOLINE_PHYS_ADDR;
	extern usize SMP_TRAMPLINE_PHYS_ENTRY16;
	extern usize SMP_TRAMPLINE_PHYS_ENTRY32;
}

void arch_sleep_init();
void arch_prepare_for_sleep();
