#pragma once
#include "interrupts/interrupts.hpp"
#include "types.hpp"
#include "utils/cpuid.hpp"
#include "debug/debug.hpp"

#ifdef WITH_SCHED_MULTI
#include "sched/sched.hpp"
#include "timer/timer.hpp"
#endif

struct CpuLocal {
	CpuLocal* self {this};
	Task* current_task {};
	Task* blocked_tasks[as<u8>(TaskStatus::Max)] {};
	SchedLevel levels[SCHED_MAX_LEVEL] {};
	Spinlock lock {};
	u8 reserved[3] {};
	atomic_uint_fast32_t thread_count = 0;
	Timer timer {};
	DebugState debug_state {};
	bool debugging {};
};

extern usize cpu_count;