#include "sched.hpp"
#include "stdio.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"

[[noreturn]] void sched_thread_destroyer_fn(void*) {
	auto cpu = get_current_thread()->cpu;
	auto& list = cpu->sched_destroy_list;
	auto& event = cpu->sched_destroy_event;

	while (true) {
		{
			IrqGuard irq_guard {};
			auto guard = list.lock();
			for (auto& thread : *guard) {
				println("[kernel][sched]: destroying exited thread ", thread.name);
				guard->remove(&thread);
				auto process = thread.process;
				process->remove_thread(&thread);
				delete &thread;
				if (process->is_empty()) {
					println("[kernel][sched]: destroying empty process ", process->name);
					delete process;
				}
			}
		}

		event.wait();
	}
}

void sched_init() {
	IrqGuard irq_guard {};
	auto cpu = get_current_thread()->cpu;
	cpu->scheduler.queue(&cpu->thread_destroyer);
}

extern "C" void sched_switch_thread(ArchThread* prev, ArchThread* next);
void sched_before_switch(Thread* prev, Thread* thread);

Scheduler::Scheduler() {
	constexpr usize US_PER_LEVEL = MAX_US / SCHED_LEVELS;

	for (usize i = 1; i < SCHED_LEVELS + 1; ++i) {
		levels[i - 1].slice_us = US_PER_LEVEL * i;
	}
}

void Scheduler::queue(Thread* thread) {
	auto& level = levels[thread->level_index];
	IrqGuard irq_guard {};
	level.list.lock()->push(thread);
}

void Scheduler::update_schedule() {
	IrqGuard irq_guard {};
	if (current->status == Thread::Status::Running) {
		if (us_to_next_schedule > current_irq_period) {
			us_to_next_schedule -= current_irq_period;
			prev = current;
			return;
		}

		if (!current->pin_level && current->level_index < SCHED_LEVELS - 1) {
			++current->level_index;
		}
	}

	Thread* next;

	auto current_thread = get_current_thread();

	for (auto& level : levels) {
		auto guard = level.list.lock();

		auto* start = guard->pop_front();
		next = start;
		if (!next) {
			continue;
		}

		while (true) {
			if (next->status == Thread::Status::Waiting) {
				goto found;
			}
			guard->push(next);
			next = guard->pop_front();
			if (next == start) {
				break;
			}
		}
	}

	found:
	if (!next) {
		if (!current_thread->process->killed && current_thread->status == Thread::Status::Running) {
			prev = current_thread;
			return;
		}
		else {
			next = &current_thread->cpu->idle_thread;
		}
	}

	prev = current;
	current = next;
	us_to_next_schedule = levels[current->level_index].slice_us;
}

void Scheduler::do_schedule() const {
	// do_schedule must always be called with interrupts disabled
	assert(!arch_enable_irqs(false));

	if (prev == current) {
		return;
	}

	if (prev->process->killed || prev->exited) {
		prev->cpu->sched_destroy_list.lock()->push(prev);
		prev->cpu->sched_destroy_event.signal_one();
	}
	else if (prev != &prev->cpu->idle_thread && prev->status == Thread::Status::Running) {
		prev->status = Thread::Status::Waiting;
		prev->cpu->scheduler.queue(prev);
	}

	current->status = Thread::Status::Running;

	current->process->page_map.use();

	set_current_thread(current);
	sched_before_switch(prev, current);
	sched_switch_thread(prev, current);
}

void Scheduler::block() {
	IrqGuard irq_guard {};
	current->status = Thread::Status::Blocked;
	current->cpu->cpu_tick_source->reset();

	if (current->level_index > 0) {
		--current->level_index;
	}

	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
}

void Scheduler::yield() {
	IrqGuard irq_guard {};
	current->status = Thread::Status::Waiting;
	current->cpu->cpu_tick_source->reset();

	if (current->level_index > 0) {
		--current->level_index;
	}

	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
}

void Scheduler::exit_process(int status) {
	IrqGuard irq_guard {};
	current->process->killed = true;
	current->process->exit(status);
	current->status = Thread::Status::Blocked;
	current->cpu->cpu_tick_source->reset();
	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
	__builtin_trap();
}

void Scheduler::exit_thread(int status) {
	IrqGuard irq_guard {};
	current->exited = true;
	current->exit(status);
	current->status = Thread::Status::Blocked;
	current->cpu->cpu_tick_source->reset();
	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
	__builtin_trap();
}

void Scheduler::unblock(Thread* thread, bool remove_sleeping) {
	IrqGuard irq_guard {};
	// thread->cpu is guaranteed to not change while its state is not running
	auto guard = thread->cpu->scheduler.levels[thread->level_index].list.lock();
	if (remove_sleeping && thread->status == Thread::Status::Sleeping) {
		sleeping_threads.lock()->remove(thread);
	}
	thread->status = Thread::Status::Waiting;
	guard->push(thread);
}

void Scheduler::on_timer(Cpu* cpu) {
	update_schedule();
	enable_preemption(cpu);
	cpu->deferred_work.push(&irq_work);
}

void Scheduler::enable_preemption(Cpu* cpu) {
	usize now;
	{
		auto guard = CLOCK_SOURCE.lock_read();
		now = (*guard)->get() / (*guard)->ticks_in_us;
	}

	usize first_sleep_end = UINTPTR_MAX;
	auto guard = sleeping_threads.lock();
	for (auto& thread : *guard) {
		if (thread.sleep_end > now) {
			first_sleep_end = thread.sleep_end;
			break;
		}
		guard->remove(&thread);
		unblock(&thread, false);
	}

	auto time_before_sleep_end = first_sleep_end - now;
	auto slice_us = levels[current->level_index].slice_us;
	auto amount = kstd::min(time_before_sleep_end, slice_us);
	current_irq_period = amount;
	cpu->cpu_tick_source->oneshot(amount);
}

void Scheduler::sleep(u64 us) {
	IrqGuard irq_guard {};
	current->cpu->cpu_tick_source->reset();

	usize sleep_end;
	{
		auto guard = CLOCK_SOURCE.lock_read();
		auto now = (*guard)->get() / (*guard)->ticks_in_us;
		sleep_end = now + us;
	}

	{
		Thread* prev_sleeping = nullptr;
		auto guard = sleeping_threads.lock();
		for (auto& thread : *guard) {
			if (thread.sleep_end > sleep_end) {
				prev_sleeping = &thread;
				break;
			}
		}

		current->sleep_end = sleep_end;
		guard->insert_before(prev_sleeping, current);
		current->status = Thread::Status::Sleeping;
	}

	if (current->level_index > 0) {
		--current->level_index;
	}

	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
}
