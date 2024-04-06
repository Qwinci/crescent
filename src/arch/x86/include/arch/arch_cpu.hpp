#pragma once
#include "arch/x86/interrupts/tss.hpp"
#include "arch/x86/dev/lapic.hpp"
#include "manually_init.hpp"

struct ArchCpu {
	Tss tss;
	ManuallyInit<LapicTickSource> lapic_timer;
	u32 lapic_id;
};
