#pragma once
#include "deferred_work.hpp"
#include "dev/clock.hpp"
#include "double_list.hpp"
#include "sched/thread.hpp"
#include "types.hpp"
#include "utils/spinlock.hpp"

struct Cpu;
struct Process;

static constexpr usize SCHED_MAX_SLEEP_US = US_IN_S * 60 * 60 * 24;

struct Scheduler {
	Scheduler();

	static constexpr usize MAX_US = 50 * US_IN_MS;
	static constexpr usize SCHED_LEVELS = 12;

	void queue(Thread* thread);
	void update_schedule();
	void do_schedule() const;

	void sleep(u64 us);
	void yield();

	void block();
	void unblock(Thread* thread, bool remove_sleeping);

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
	u64 current_irq_period {};
	u64 us_to_next_schedule {};
	DeferredIrqWork irq_work {.fn = [this]() {
		do_schedule();
	}};
	Spinlock<DoubleList<Thread, &Thread::hook>> sleeping_threads {};
};

void sched_init();
Thread* get_current_thread();
void set_current_thread(Thread* thread);
