#pragma once
#include "memory/map.hpp"
#include "types.hpp"
#include "utils/spinlock.hpp"

constexpr usize US_IN_MS = 1000;
constexpr usize US_IN_SEC = 1000 * 1000;

enum class TaskStatus : u8 {
	Ready = 1,
	Running = 0,
	Sleeping = 2,
	Exited = 3,
	Killed = 4
};

struct Task {
	void decrease_level() {
		if (task_level > 0) {
			task_level -= 1;
		}
	}

	[[nodiscard]] PageMap* get_map() const {
		return cast<PageMap*>(PhysAddr {map}.to_virt().as_usize());
	}

	char name[128];
	u64 rsp;
	u64 kernel_rsp;
	Task* next;
	u64 map;
	u64 sleep_end;
	u64 stack_base;
	u64 stack_size;
	TaskStatus status;
	u8 task_level;
	bool user;
	u8 reserved[5];
};

#define SCHED_MAX_LEVEL 32
#define SCHED_SLICE_MAX_US (50 * US_IN_MS)
#define SCHED_SLICE_MIN_US (6 * US_IN_MS)

struct SchedLevel {
	[[nodiscard]] bool is_empty() const {
		return !ready_tasks;
	}

	Task* ready_tasks;
	Task* ready_tasks_end;
	usize slice_us;
};

void sched_init();

void sched_block(TaskStatus status);
void sched_unblock(Task* task);
void sched_sleep(u64 us);
void sched_kill();
[[noreturn]] void sched_exit();

void sched_queue_task(Task* task);
void sched();
Task* create_kernel_task(const char* name, void (*fn)());
Task* create_user_task(const char* name, PageMap* map, void (*fn)(), void* arg);
extern Task* current_task;