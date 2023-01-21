#include "scheduler.hpp"

Process* ready_to_run_tasks {};
Process* ready_to_run_tasks_end {};
Process* current_task {};
Spinlock scheduler_lock {};

extern "C" void task_startup() {
	println("task startup: unlocking scheduler");
	scheduler_lock.unlock();
}

Process* create_kernel_task(void (*task)()) {
	u8* stack = new u8[0x1000];
	stack += 0x1000 - 8;
	// task rip
	*(u64*) stack = (u64) task;
	stack -= 8;
	// startup rip
	*(u64*) stack = (u64) task_startup;
	stack -= 8;
	// rbx
	*(u64*) stack = 0;
	stack -= 8;
	// rbp
	*(u64*) stack = 0;
	stack -= 8;
	// r12
	*(u64*) stack = 0;
	stack -= 8;
	// r13
	*(u64*) stack = 0;
	stack -= 8;
	// r14
	*(u64*) stack = 0;
	stack -= 8;
	// r15
	*(u64*) stack = 0;

	auto* process = new Process();
	process->rsp = (u64) stack;
	process->kernel_rsp = (u64) new u8[0x1000];
	process->kernel_rsp += 0x1000;
	process->map = VirtAddr {get_map()}.to_phys().as_usize();
	process->state = ProcessState::ReadyToRun;
	return process;
}

static u64 tsc_frequency = 0;

void update_kernel_time() {
	auto current = read_timestamp();
	auto elapsed = current - current_task->last_kernel_timestamp;
	current_task->kernel_cpu_time_used_ns += elapsed / (tsc_frequency * 1000000000ULL);
	current_task->last_kernel_timestamp = current;
}

void update_user_time() {
	auto current = read_timestamp();
	auto elapsed = current - current_task->last_user_timestamp;
	current_task->user_cpu_time_used_ns += elapsed / (tsc_frequency * 1000000000ULL);
	current_task->last_user_timestamp = current;
}

atomic_size_t delay_task_switch = 0;
bool task_switch_delayed = false;

/// NOTE: lock scheduler before calling
void schedule() {
	if (delay_task_switch) {
		task_switch_delayed = true;
		return;
	}

	if (ready_to_run_tasks) {
		auto task = ready_to_run_tasks;
		ready_to_run_tasks = task->next;
		switch_task(task);
	}
	else if (current_task->state != ProcessState::Running) {
		auto task = current_task;
		current_task = nullptr;
		u64 idle_start = read_timestamp();

		do {
			enable_interrupts();
			asm volatile("hlt");
			disable_interrupts();
		} while (!ready_to_run_tasks);

		current_task = task;

		task = ready_to_run_tasks;
		ready_to_run_tasks = task->next;
		if (task != current_task) {
			switch_task(task);
		}
	}
}

void schedule_kernel_task(Process* task) {
	if (!ready_to_run_tasks) {
		ready_to_run_tasks = task;
		ready_to_run_tasks_end = task;
	}
	else {
		ready_to_run_tasks_end->next = task;
		ready_to_run_tasks_end = task;
	}
	schedule();
}

Spinlock scheduler_misc_lock;

void lock_scheduler_misc() {
	scheduler_misc_lock.lock();
	delay_task_switch += 1;
}

void unlock_scheduler_misc() {
	delay_task_switch -= 1;
	if (delay_task_switch == 0) {
		scheduler_misc_lock.unlock();

		if (task_switch_delayed) {
			task_switch_delayed = false;
			schedule();
		}
	}
}

void block_task(ProcessState reason) {
	scheduler_lock.lock();
	current_task->state = reason;
	schedule();
	scheduler_lock.unlock();
}

void unblock_task(Process* process) {
	scheduler_lock.lock();
	// todo priorities
	if (!ready_to_run_tasks) {
		switch_task(process);
	}
	else {
		ready_to_run_tasks_end->next = process;
		ready_to_run_tasks_end = process;
	}
	scheduler_lock.unlock();
}

void scheduler_init() {
	tsc_frequency = get_cpu_local()->tsc_frequency;
}

Process* sleeping_tasks;
Process* sleeping_tasks_end;

void nano_sleep_until(u64 timestamp) {
	lock_scheduler_misc();

	u64 time;
	if (tsc_frequency) {
		time = read_timestamp() / (tsc_frequency * 1000000000ULL);
	}
	else {
		time = get_current_timer_ns();
	}

	if (timestamp <= time) {
		scheduler_lock.unlock();
		return;
	}

	current_task->sleep_end = timestamp;

	if (!sleeping_tasks) {
		current_task->next = nullptr;
		sleeping_tasks = current_task;
		sleeping_tasks_end = current_task;
	}
	else {
		if (sleeping_tasks->sleep_end >= timestamp) {
			current_task->next = sleeping_tasks;
			sleeping_tasks = current_task;
		}
		else if (timestamp >= sleeping_tasks_end->sleep_end) {
			current_task->next = nullptr;
			sleeping_tasks_end->next = current_task;
			sleeping_tasks_end = current_task;
		}
		else {
			Process* process = sleeping_tasks;
			while (true) {
				if (process->next->sleep_end > timestamp) {
					current_task->next = process->next;
					process->next = current_task;
					break;
				}
				process = process->next;
			}
		}
	}

	unlock_scheduler_misc();

	block_task(ProcessState::Sleeping);
}