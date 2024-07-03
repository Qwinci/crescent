#pragma once
#include "arch/arch_cpu.hpp"
#include "sched/sched.hpp"
#include "dev/clock.hpp"
#include "sched/process.hpp"
#include "sched/deferred_work.hpp"

[[noreturn]] void arch_idle_fn(void*);
[[noreturn]] void sched_thread_destroyer_fn(void*);

struct Cpu : public ArchCpu {
	Scheduler scheduler;
	Thread idle_thread {"idle", this, &*KERNEL_PROCESS, arch_idle_fn, nullptr};
	Thread thread_destroyer {"thread destroyer", this, &*KERNEL_PROCESS, sched_thread_destroyer_fn, nullptr};
	Spinlock<DoubleList<Thread, &Thread::hook>> sched_destroy_list {};
	Event sched_destroy_event {};
	TickSource* cpu_tick_source {};
	DoubleList<DeferredIrqWork, &DeferredIrqWork::hook> deferred_work {};
	int number {};
	kstd::atomic<u32> thread_count {};
};

usize arch_get_cpu_count();
Cpu* arch_get_cpu(usize index);
