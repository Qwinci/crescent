#pragma once
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "drivers/tsc.hpp"
#include "memory/map.hpp"
#include "timer/timer.hpp"

enum class ProcessState : u64 {
	ReadyToRun = 0,
	Running = 1,
	Sleeping = 2
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
	u64 sleep_end;
};
static_assert(sizeof(Process) == 80);

/// NOTE: lock scheduler before calling
extern "C" void switch_task(Process* process);

Process* create_kernel_task(void (*task)());

template<typename T>
struct AtomicLinkedList {
	struct Node {
		T data;
		Node* next;
	};
	Node* root;
	Node* end;

	void push_back(Node* node) {
		node->next = nullptr;

		if (atomic_compare_exchange_strong_explicit(end, nullptr, node, memory_order_acq_rel, memory_order_acquire)) {
			return;
		}

		while (true) {
			volatile auto e = end;
			if (atomic_compare_exchange_strong_explicit(e->next, nullptr, node, memory_order_acq_rel, memory_order_acquire)) {
				break;
			}
		}
	}

	Node* pop_front() {
		if (atomic_load_explicit(root)) {
			while (true) {
				volatile auto node = root;
				if (atomic_compare_exchange_strong_explicit(root, node->next, node->next, memory_order_acq_rel, memory_order_acquire)) {
					break;
				}
			}
		}
		else {
			return nullptr;
		}
	}
};

extern Process* ready_to_run_tasks;
extern Process* ready_to_run_tasks_end;
extern Process* current_task;
extern Spinlock scheduler_lock;

void update_kernel_time();
void update_user_time();
void nano_sleep_until(u64 timestamp);

extern Spinlock scheduler_misc_lock;

/// NOTE: lock scheduler before calling
void schedule();

static void test_kernel_task() {
	println("hello from kernel task");
	nano_sleep_until(get_current_timer_ns() + 1000000000ULL * 2);
	println("test kernel task resumed :p");
}

void schedule_kernel_task(Process* task);

void lock_scheduler_misc();
void unlock_scheduler_misc();
void block_task(ProcessState reason);
void unblock_task(Process* process);
void scheduler_init();

static inline void test_task() {
	println("testing scheduler");
	current_task = new Process();
	current_task->state = ProcessState::Running;
	current_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	auto new_task = create_kernel_task(test_kernel_task);
	schedule_kernel_task(new_task);
	println("switching task");
	println("back in main kernel");
	println("doing second run");
	println("back from second run");
}