#pragma once
#include "sched/sched.h"
#include "sched/task.h"
#include "types.h"
#include "utils/spinlock.h"
#include <stdatomic.h>

typedef struct Cpu {
	usize idle_start;
	usize idle_time;
	atomic_size_t thread_count;
	SchedLevel sched_levels[SCHED_MAX_LEVEL];
	Task* current_task;
	Task* idle_task;
	Task* blocked_tasks[TASK_STATUS_MAX];
	void* cur_map;
	Spinlock lock;
} Cpu;

void arch_init_smp();
usize arch_get_cpu_count();
Task* arch_get_cur_task();
Cpu* arch_get_cpu(usize index);
void arch_hlt_cpu(usize index);
