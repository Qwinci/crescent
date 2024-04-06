#pragma once
#include "deferred_work.hpp"
#include "dev/clock.hpp"
#include "double_list.hpp"
#include "sched/thread.hpp"
#include "types.hpp"
#include "utils/spinlock.hpp"

struct Cpu;
struct Process;

struct Scheduler {
	Scheduler();

	static constexpr usize MAX_US = 50/* * US_IN_MS*/;
	static constexpr usize SCHED_LEVELS = 12;

	void queue(Thread* thread);
	void update_schedule();
	void do_schedule() const;

	void sleep(u64 us);

	void block();
	void unblock(Thread* thread);

	[[noreturn]] void exit_process(int status);
	[[noreturn]] void exit_thread(int status);

	void enable_preemption(Cpu* cpu);
	void on_timer(Cpu* cpu);

	struct Level {
		Spinlock<DoubleList<Thread, &Thread::hook>> list {};
		usize slice_us;
	};

	Level levels[SCHED_LEVELS] {};

	Thread* prev {};
	Thread* current {};
	DeferredIrqWork irq_work {.fn = [this]() {
		do_schedule();
	}};
	DoubleList<Thread, &Thread::hook> sleeping_threads {};
};

void sched_init();
Thread* get_current_thread();
void set_current_thread(Thread* thread);
