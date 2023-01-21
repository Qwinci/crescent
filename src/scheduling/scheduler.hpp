#pragma once
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "drivers/tsc.hpp"
#include "memory/map.hpp"

enum class ProcessState : u64 {
	ReadyToRun = 0,
	Running = 1
};

struct Process {
	u64 rsp;
	u64 kernel_rsp;
	u64 map;
	Process* next;
	ProcessState state;
	u64 user_cpu_time_used_ns;
	u64 last_user_timestamp;
	u64 kernel_cpu_time_used_ns;
	u64 last_kernel_timestamp;
};
static_assert(sizeof(Process) == 72);

extern "C" void switch_task(Process* process);

Process* create_kernel_task(void (*task)());

extern Process* ready_to_run_tasks;
extern Process* ready_to_run_tasks_end;
extern Process* current_task;

static inline void update_kernel_time() {
	auto current = read_timestamp();
	auto elapsed = current - current_task->last_kernel_timestamp;
	current_task->kernel_cpu_time_used_ns += elapsed / (get_cpu_local()->tsc_frequency * 1000000000ULL);
	current_task->last_kernel_timestamp = current;
}

static inline void update_user_time() {
	auto current = read_timestamp();
	auto elapsed = current - current_task->last_user_timestamp;
	current_task->user_cpu_time_used_ns += elapsed / (get_cpu_local()->tsc_frequency * 1000000000ULL);
	current_task->last_user_timestamp = current;
}

static inline void schedule() {
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

static void test_kernel_task() {
	println("hello from kernel task");
	schedule();
	println("test kernel task resumed :p");
	schedule();
}

static inline void schedule_kernel_task(Process* task) {
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

static inline void test_task() {
	println("testing scheduler");
	current_task = new Process();
	current_task->state = ProcessState::Running;
	current_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	auto new_task = create_kernel_task(test_kernel_task);
	println("switching task");
	schedule_kernel_task(new_task);
	println("back in main kernel");
	println("doing second run");
	schedule();
	println("back from second run");
}