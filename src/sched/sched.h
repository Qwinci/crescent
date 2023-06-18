#pragma once
#include "task.h"
#include "types.h"
#include "utils/attribs.h"

#define NS_IN_US 1000
#define US_IN_MS 1000
#define US_IN_SEC (1000 * 1000)

#define SCHED_MAX_LEVEL 32
#define SCHED_SLICE_MAX_US (50 * US_IN_MS)
#define SCHED_SLICE_MIN_US (6 * US_IN_MS)

typedef struct Task Task;

typedef struct {
	Task* ready_tasks;
	Task* ready_tasks_end;
	usize slice_us;
} SchedLevel;

Task* arch_create_kernel_task(const char* name, void (*fn)(), void* arg);
Task* arch_create_user_task(const char* name, void (*fn)(), void* arg, Task* parent, bool detach);
Task* arch_create_user_task_with_map(const char* name, void (*fn)(), void* arg, Task* parent, void* map, struct TaskVMem* vmem, bool detach);
void arch_set_user_task_fn(Task* task, void (*fn)());
void arch_destroy_task(Task* task);

void sched();
void sched_queue_task(Task* task);
void sched_block(TaskStatus status);
bool sched_unblock(Task* task);
void sched_sleep(usize us);
NORETURN void sched_exit(int status, TaskStatus type);
NORETURN void sched_kill_cur();
void sched_kill_child(Task* task);
void sched_sigwait(Task* task);
void sched_invalidate_map(Task *self);
