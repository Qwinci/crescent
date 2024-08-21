#pragma once
#include "arch/aarch64/dev/timer.hpp"

struct ArchCpu {
	ArmTickSource arm_tick_source {};
	u32 affinity {};
};
