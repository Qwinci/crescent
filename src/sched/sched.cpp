#include "sched.hpp"
#include "stdio.hpp"
#include "arch/cpu.hpp"
#include "assert.hpp"

[[noreturn]] void sched_load_balancer_fn(void*) {
	auto& scheduler = get_current_thread()->cpu->scheduler;

	usize cpu_count = arch_get_cpu_count();

	while (true) {
		u32 min_thread_count = UINT32_MAX;
		u32 max_thread_count = 0;
		usize min_cpu_index = 0;
		usize max_cpu_index = 0;

		for (usize i = 0; i < cpu_count; ++i) {
			auto* cpu = arch_get_cpu(i);
			u32 thread_count = cpu->thread_count.load(kstd::memory_order::relaxed);
			if (thread_count < min_thread_count) {
				min_thread_count = thread_count;
				min_cpu_index = i;
			}
			if (thread_count > max_thread_count) {
				max_thread_count = thread_count;
				max_cpu_index = i;
			}
		}

		u32 diff = max_thread_count - min_thread_count;
		if (min_cpu_index != max_cpu_index && diff >= 2) {
			auto* min_cpu = arch_get_cpu(min_cpu_index);
			auto* max_cpu = arch_get_cpu(max_cpu_index);

			IrqGuard irq_guard {};
			for (usize i = Scheduler::SCHED_LEVELS; i > 0 && diff; --i) {
				auto guard = max_cpu->scheduler.levels[i - 1].list.lock();

				for (auto& thread : *guard) {
					auto thread_guard = thread.sched_lock.lock();

					if (!thread.pin_cpu && thread.status == Thread::Status::Waiting) {
						println("[kernel][sched]: moving thread ", thread.name, " from cpu ", max_cpu_index, " to cpu ", min_cpu_index);

						guard->remove(&thread);

						thread.cpu = min_cpu;
						min_cpu->scheduler.queue(&thread);

						max_cpu->thread_count.fetch_sub(1, kstd::memory_order::seq_cst);
						min_cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
						--diff;

						if (!diff) {
							break;
						}
					}
				}
			}

			if (diff) {
				auto guard = max_cpu->scheduler.sleeping_threads.lock();
				for (auto& thread : *guard) {
					if (!thread.sched_lock.try_lock()) {
						continue;
					}

					if (!thread.pin_cpu) {
						println(
							"[kernel][sched]: moving sleeping thread ",
							thread.name,
							" from cpu ",
							max_cpu_index,
							" to cpu ",
							min_cpu_index);
						guard->remove(&thread);

						thread.cpu = min_cpu;
						auto min_guard = min_cpu->scheduler.sleeping_threads.lock();

						Thread* next_sleeping = nullptr;
						for (auto& min_thread : *min_guard) {
							if (min_thread.sleep_end > thread.sleep_end) {
								next_sleeping = &min_thread;
								break;
							}
						}

						if (next_sleeping) {
							min_guard->insert_before(next_sleeping, &thread);
						}
						else {
							min_guard->push(&thread);
						}

						max_cpu->thread_count.fetch_sub(1, kstd::memory_order::seq_cst);
						min_cpu->thread_count.fetch_add(1, kstd::memory_order::seq_cst);
						--diff;

						if (!diff) {
							thread.sched_lock.manual_unlock();
							break;
						}
					}

					thread.sched_lock.manual_unlock();
				}
			}
		}

		auto state = scheduler.prepare_for_sleep(US_IN_S);
		scheduler.sleep(state);
	}
}

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

namespace {
	Thread* SCHED_LOAD_BALANCE_THREAD;
}

void sched_init(bool bsp) {
	IrqGuard irq_guard {};
	auto cpu = get_current_thread()->cpu;
	cpu->thread_destroyer.pin_level = true;
	cpu->thread_destroyer.pin_cpu = true;
	cpu->scheduler.queue(&cpu->thread_destroyer);

	if (bsp) {
		SCHED_LOAD_BALANCE_THREAD = new Thread {
			"load balancer",
			cpu,
			&*KERNEL_PROCESS,
			sched_load_balancer_fn,
			nullptr};
		SCHED_LOAD_BALANCE_THREAD->pin_level = true;
		SCHED_LOAD_BALANCE_THREAD->pin_cpu = true;
		cpu->scheduler.queue(SCHED_LOAD_BALANCE_THREAD);
	}
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
			if (next->process->killed) {
				next->cpu->thread_count.fetch_sub(1, kstd::memory_order::seq_cst);
				next->cpu->sched_destroy_list.lock()->push(next);
				next->cpu->sched_destroy_event.signal_one();
				next = guard->pop_front();
				if (!next) {
					break;
				}
				continue;
			}
			else if (next->status == Thread::Status::Waiting) {
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

	if (!prev->sched_lock.is_locked()) {
		prev->sched_lock.manual_lock();
	}

	if (prev->process->killed || prev->exited) {
		prev->cpu->thread_count.fetch_sub(1, kstd::memory_order::seq_cst);
		prev->cpu->sched_destroy_list.lock()->push(prev);
		prev->cpu->sched_destroy_event.signal_one();
	}
	else if (prev != &prev->cpu->idle_thread && prev->status == Thread::Status::Running) {
		assert(prev != current);
		prev->status = Thread::Status::Waiting;
		prev->cpu->scheduler.queue(prev);
	}

	current->status = Thread::Status::Running;

	current->process->cpu_set.lock()->set(current->cpu->number);

	current->process->page_map.use();

	set_current_thread(current);
	sched_before_switch(prev, current);
	sched_switch_thread(prev, current);
}

bool Scheduler::prepare_for_block() {
	auto state = arch_enable_irqs(false);

	current->sched_lock.manual_lock();
	current->status = Thread::Status::Blocked;
	current->cpu->cpu_tick_source->reset();

	if (!current->pin_level && current->level_index > 0) {
		--current->level_index;
	}
	us_to_next_schedule = 0;

	return state;
}

void Scheduler::block(bool prepare_state) {
	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
	current->sched_lock.manual_unlock();
	arch_enable_irqs(prepare_state);
}

void Scheduler::yield() {
	IrqGuard irq_guard {};
	current->cpu->cpu_tick_source->reset();

	if (!current->pin_level && current->level_index > 0) {
		--current->level_index;
	}
	us_to_next_schedule = 0;

	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
}

void Scheduler::exit_process(int status) {
	IrqGuard irq_guard {};
	auto current_guard = current->sched_lock.lock();

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
	auto current_guard = current->sched_lock.lock();

	current->exited = true;
	current->exit(status);
	current->status = Thread::Status::Blocked;
	current->cpu->cpu_tick_source->reset();

	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
	__builtin_trap();
}

void Scheduler::unblock(Thread* thread, bool remove_sleeping, bool assert_not_sleeping) {
	IrqGuard irq_guard {};
	assert(thread->status == Thread::Status::Blocked ||
		   thread->status == Thread::Status::Sleeping);
	assert(thread->sched_lock.is_locked());
	assert(thread != thread->cpu->scheduler.current);

	if (remove_sleeping && thread->status == Thread::Status::Sleeping) {
		/*
		auto guard = thread->cpu->scheduler.sleeping_threads.lock();
		guard->remove(thread);
		thread->status = Thread::Status::Blocked;
		thread->sleep_interrupted = true;
		*/
		thread->sleep_end = SIZE_MAX;
		thread->sleep_interrupted = true;
	}
	else {
		if (assert_not_sleeping) {
			assert(thread->status == Thread::Status::Blocked);
		}

		auto guard = thread->cpu->scheduler.levels[thread->level_index].list.lock();

		assert(thread != thread->cpu->scheduler.current);
		thread->status = Thread::Status::Waiting;
		guard->push(thread);
	}
}

void Scheduler::on_timer(Cpu* cpu) {
	assert(current->status == Thread::Status::Running);
	update_schedule();
	enable_preemption(cpu);
	cpu->deferred_work.push(&irq_work);
}

void Scheduler::enable_preemption(Cpu* cpu) {
	usize now;
	{
		auto guard = CLOCK_SOURCE.lock_read();
		now = (*guard)->get_ns() / NS_IN_US;
	}

	usize first_sleep_end = UINTPTR_MAX;
	auto guard = sleeping_threads.lock();

	/*
	for (auto& thread : *guard) {
		if (thread.sleep_end > now) {
			first_sleep_end = thread.sleep_end;
			break;
		}
		guard->remove(&thread);

		if (&thread != prev) {
			auto thread_guard = thread.sched_lock.lock();
			unblock(&thread, false, false);
		}
		else {
			unblock(&thread, false, false);
		}
	}
	*/

	// todo this needs a better solution, sleeping threads can't
	// be removed from the sleep list in unblock because it could create a deadlock
	// with the thread lock being acquired before the sleeping threads lock.
	// Looping through all threads here and setting the sleep end to SIZE_MAX works but is not ideal.
	for (auto& thread : *guard) {
		if (thread.sleep_end <= now || thread.sleep_end == SIZE_MAX) {
			guard->remove(&thread);
			if (&thread != prev) {
				auto thread_guard = thread.sched_lock.lock();
				unblock(&thread, false, false);
			}
			else {
				unblock(&thread, false, false);
			}

			continue;
		}

		if (first_sleep_end == UINTPTR_MAX) {
			first_sleep_end = thread.sleep_end;
		}
	}

	auto time_before_sleep_end = first_sleep_end - now;
	auto slice_us = levels[current->level_index].slice_us;
	auto amount = kstd::min(time_before_sleep_end, slice_us);
	current_irq_period = amount;
	cpu->cpu_tick_source->oneshot(amount);
}

bool Scheduler::prepare_for_sleep(u64 us) {
	auto state = arch_enable_irqs(false);
	current->cpu->cpu_tick_source->reset();

	if (!us) {
		return state;
	}

	usize sleep_end;
	{
		auto guard = CLOCK_SOURCE.lock_read();
		auto now = (*guard)->get_ns() / NS_IN_US;
		sleep_end = now + us;
	}

	current->sched_lock.manual_lock();
	{
		Thread* next_sleeping = nullptr;
		auto guard = sleeping_threads.lock();
		for (auto& thread : *guard) {
			if (thread.sleep_end > sleep_end) {
				next_sleeping = &thread;
				break;
			}
		}

		current->sleep_end = sleep_end;

		if (next_sleeping) {
			guard->insert_before(next_sleeping, current);
		}
		else {
			guard->push(current);
		}

		current->status = Thread::Status::Sleeping;
	}

	if (!current->pin_level && current->level_index > 0) {
		--current->level_index;
	}

	return state;
}

void Scheduler::sleep(bool prepare_state) {
	update_schedule();
	enable_preemption(current->cpu);
	do_schedule();
	current->sched_lock.manual_unlock();
	arch_enable_irqs(prepare_state);
}
