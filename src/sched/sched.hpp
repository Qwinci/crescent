#pragma once
#include "memory/map.hpp"
#include "types.hpp"
#include "utils/spinlock.hpp"

enum class TaskStatus : u8 {
	Ready = 1,
	Running = 0,
	Sleeping = 2
};

struct Task {
	char name[128];
	u64 rsp;
	u64 kernel_rsp;
	Task* next;
	u64 map;
	TaskStatus status;
	u8 reserved[7];
	u64 sleep_end;
};

static_assert(sizeof(Task) == 128 + sizeof(u64) + 8 + 8 + 8 + 8 + 8);

void test_sched();
void sched_init();
void sched_queue_task(Task* task);
void sched();
extern Spinlock sched_lock;
Task* create_kernel_task(const char* name, void (*fn)());
void unblock_task(Task* task);