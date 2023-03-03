#include "mutex.hpp"
#include "arch.hpp"
#include "interrupts/interrupts.hpp"
#include "sched.hpp"

bool Mutex::try_lock() {
	return !atomic_exchange_explicit(&inner, true, memory_order_acquire);
}

void Mutex::lock() {
	auto result = !atomic_exchange_explicit(&inner, true, memory_order_acquire);
	if (!result) {
		auto flags = enter_critical();
		auto local = arch_get_cpu_local();
		auto task = local->current_task;
		task->status = TaskStatus::Waiting;
		task->next = waiting_tasks;
		waiting_tasks = task;
		leave_critical(flags);
		sched();
	}
}

void Mutex::unlock() {
	auto flags = enter_critical();
	atomic_store_explicit(&inner, false, memory_order_release);

	for (; waiting_tasks; waiting_tasks = waiting_tasks->next) {
		sched_unblock(waiting_tasks);
	}

	leave_critical(flags);
}
