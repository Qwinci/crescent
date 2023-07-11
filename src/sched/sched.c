#include "sched.h"
#include "arch/cpu.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "dev/timer.h"
#include "mutex.h"
#include "sched_internals.h"
#include "task.h"
#include "utils/attribs.h"
#include "mem/vmem.h"

[[noreturn]] static void idle_task_fn(void*) {
	while (true) {
		arch_hlt();
		Ipl old = arch_ipl_set(IPL_CRITICAL);
		Cpu* cpu = arch_get_cur_task()->cpu;
		spinlock_lock(&cpu->tasks_lock);
		Task* task = sched_get_next_task();
		spinlock_unlock(&cpu->tasks_lock);
		if (task) {
			sched_with_next(task);
		}
		arch_ipl_set(old);
	}
}

// Precondition: ipl is critical or interrupts are disabled
void sched_switch_from(Task* old_task, Task* self) {
	Task* t = arch_get_cur_task();
	Cpu* cpu = t->cpu;

	if (old_task->status == TASK_STATUS_RUNNING) {
		if (old_task == cpu->idle_task) {
			cpu->idle_time += arch_get_ns_since_boot() - cpu->idle_start;
		}
		else {
			sched_queue_task(old_task);
		}
	}
	else if (old_task->status == TASK_STATUS_EXITED || old_task->status == TASK_STATUS_KILLED) {
		cpu->thread_count -= 1;
		arch_destroy_task(old_task);
	}

	self->status = TASK_STATUS_RUNNING;

	arch_sched_switch_from(old_task, self);
}

void sched_switch_from_init(Task* old_task) {
	Task* self = arch_get_cur_task();
	if (self != self->cpu->idle_task) {
		self->cpu->thread_count += 1;
	}
	sched_switch_from(old_task, arch_get_cur_task());
}

static Task* get_next_task_from_level(u8 level_num) {
	Cpu* cpu = arch_get_cur_task()->cpu;

	SchedLevel* level = &cpu->sched_levels[level_num];

	if (level->ready_tasks) {
		Task* task = level->ready_tasks;
		level->ready_tasks = task->next;
		return task;
	}
	else {
		return NULL;
	}
}

Task* sched_get_next_task() {
	Task* task = NULL;
	for (usize i = SCHED_MAX_LEVEL; i > 1; --i) {
		task = get_next_task_from_level(i - 1);
		if (task) {
			break;
		}
	}
	return task;
}

void sched_with_next(Task* task) {
	Cpu* cpu = arch_get_cur_task()->cpu;

	if (!task) {
		if (cpu->current_task->status != TASK_STATUS_RUNNING) {
			task = cpu->idle_task;
			cpu->idle_start = arch_get_ns_since_boot();
		}
		else {
			return;
		}
	}

	Task* self = cpu->current_task;
	cpu->current_task = task;

	arch_switch_task(self, cpu->current_task);
}

void sched() {
	Cpu* cpu = arch_get_cur_task()->cpu;
	// This is a spinlock and not a mutex because only this cpu might access it (along with the load balancer)
	// so if it would be a mutex it would try to reschedule in case of load balancing and then block again at this same point
	spinlock_lock(&cpu->tasks_lock);

	// Get the next task or idle if current task doesn't want to run anymore
	Task* task = sched_get_next_task();

	// Now we have gotten a task, so we don't need the lock anymore
	spinlock_unlock(&cpu->tasks_lock);
	sched_with_next(task);
}

void sched_queue_task_for_cpu(Task* task, Cpu* cpu) {

	task->status = TASK_STATUS_READY;
	task->next = NULL;
	task->cpu = cpu;

	SchedLevel* level = &cpu->sched_levels[task->level];
	if (!level->ready_tasks) {
		level->ready_tasks = task;
		level->ready_tasks_end = task;
	}
	else {
		level->ready_tasks_end->next = task;
		level->ready_tasks_end = task;
	}
}

void sched_queue_task(Task* task) {
	Cpu* cpu = arch_get_cur_task()->cpu;
	spinlock_lock(&cpu->tasks_lock);
	sched_queue_task_for_cpu(task, cpu);
	spinlock_unlock(&cpu->tasks_lock);
}

void sched_block(TaskStatus status) {
	arch_ipl_set(IPL_CRITICAL);
	arch_get_cur_task()->status = status;
	sched();
}

void sched_sigwait(Task* task) {
	Task* self = arch_get_cur_task();
	arch_ipl_set(IPL_CRITICAL);

	self->status = TASK_STATUS_WAITING;
	self->next = task->signal_waiters;
	task->signal_waiters = self;

	sched();
}

bool sched_unblock(Task* task) {
	if (task->status <= TASK_STATUS_READY) {
		return false;
	}

	if (task->level < SCHED_MAX_LEVEL - 1) {
		task->level += 1;
	}
	Ipl old = arch_ipl_set(IPL_CRITICAL);

	Task* current_task = arch_get_cur_task();
	u16 priority = current_task->level + current_task->priority;
	u16 task_priority = task->level + task->priority;
	sched_queue_task(task);

	arch_ipl_set(old);
	return priority < task_priority;
}

void sched_sleep(usize us) {
	Ipl old = arch_ipl_set(IPL_CRITICAL);

	Task* self = arch_get_cur_task();

	usize end = arch_get_ns_since_boot() / NS_IN_US + us;
	self->status = TASK_STATUS_SLEEPING;
	self->sleep_end = end;

	if (!self->cpu->blocked_tasks[TASK_STATUS_SLEEPING]) {
		self->cpu->blocked_tasks[TASK_STATUS_SLEEPING] = self;
		self->cpu->current_task->next = NULL;
	}
	else {
		Task* task;
		for (task = self->cpu->blocked_tasks[TASK_STATUS_SLEEPING]; task->next; task = task->next) {
			if (task->next->sleep_end > end) {
				break;
			}
		}
		self->next = task->next;
		task->next = self;
	}

	sched_block(TASK_STATUS_SLEEPING);
	arch_ipl_set(old);
}

NORETURN void sched_exit(int status, TaskStatus type) {
	Task* self = arch_get_cur_task();

	Ipl old = arch_ipl_set(IPL_CRITICAL);

	while (self->signal_waiters) {
		Task* next = self->signal_waiters->next;
		sched_unblock(self->signal_waiters);
		self->signal_waiters = next;
	}

	self->status = type;
	if (self->process) {
		ThreadHandle* handle = handle_tab_open(&self->process->handle_table, self->tid);
		handle->exited = true;
		handle->status = status;
		// todo fix sync
	}

	spinlock_lock(&ACTIVE_INPUT_TASK_LOCK);
	if (ACTIVE_INPUT_TASK == self) {
		ACTIVE_INPUT_TASK = NULL;
	}
	spinlock_unlock(&ACTIVE_INPUT_TASK_LOCK);

	arch_ipl_set(old);
	sched();
	__builtin_unreachable();
}

NORETURN void sched_kill_cur() {
	sched_exit(-1, TASK_STATUS_KILLED);
}

NORETURN void sched_load_balance(void*) {
	while (true) {
		usize min_thread_count = UINTPTR_MAX;
		usize max_thread_count = 0;
		u8 min_cpu_i = 0;
		u8 max_cpu_i = 0;

		for (usize i = 0; i < arch_get_cpu_count(); ++i) {
			Cpu* cpu = arch_get_cpu(i);
			u32 thread_count = atomic_load_explicit(&cpu->thread_count, memory_order_relaxed);
			if (thread_count < min_thread_count) {
				min_thread_count = thread_count;
				min_cpu_i = i;
			}
			if (thread_count > max_thread_count) {
				max_thread_count = thread_count;
				max_cpu_i = i;
			}
		}

		Ipl old = arch_ipl_set(IPL_CRITICAL);
		usize diff = max_thread_count - min_thread_count;
		if (min_cpu_i != max_cpu_i && diff > 1) {
			Cpu* min_cpu = arch_get_cpu(min_cpu_i);
			Cpu* max_cpu = arch_get_cpu(max_cpu_i);

			spinlock_lock(&min_cpu->tasks_lock);
			spinlock_lock(&max_cpu->tasks_lock);

			// todo better names since these don't hlt anymore
			arch_hlt_cpu(min_cpu_i);
			arch_hlt_cpu(max_cpu_i);

			usize amount = 0;
			for (usize i = SCHED_MAX_LEVEL; i > 0 && diff > 1; --i) {
				SchedLevel* level = &max_cpu->sched_levels[i - 1];

				Task* prev = NULL;
				Task* next = NULL;
				for (Task* task = level->ready_tasks; task; task = next) {
					next = task->next;
					if (!task->pin_cpu) {
						diff -= 1;
						if (prev) {
							prev->next = next;
						}
						else {
							level->ready_tasks = next;
						}

						sched_queue_task_for_cpu(task, min_cpu);
						++amount;
					}
					else {
						prev = task;
					}
				}
			}

			max_cpu->thread_count -= amount;
			min_cpu->thread_count += amount;

			spinlock_unlock(&min_cpu->tasks_lock);
			spinlock_unlock(&max_cpu->tasks_lock);
		}

		arch_ipl_set(old);
		sched_sleep(US_IN_SEC);
	}
}

static Task* load_balance_task;

static void create_kernel_tasks(bool bsp) {
	Cpu* cpu = arch_get_cur_task()->cpu;
	cpu->idle_task = arch_create_kernel_task("<idle>", idle_task_fn, NULL);
	cpu->idle_task->cpu = cpu;
	cpu->idle_task->level = 0;
	cpu->idle_task->pin_cpu = true;
	cpu->idle_task->pin_level = true;
	if (bsp) {
		load_balance_task = arch_create_kernel_task("<load balance>", sched_load_balance, NULL);
		load_balance_task->cpu = cpu;
		load_balance_task->pin_cpu = true;
		load_balance_task->pin_level = true;
		sched_queue_task(load_balance_task);
	}

	usize inc = (SCHED_SLICE_MAX_US - SCHED_SLICE_MIN_US) / SCHED_MAX_LEVEL;

	for (usize i = 0; i < SCHED_MAX_LEVEL; ++i) {
		cpu->sched_levels[i].slice_us = (SCHED_MAX_LEVEL - i) * inc;
	}
}

void sched_init(bool bsp) {
	Ipl old = arch_ipl_set(IPL_CRITICAL);

	create_kernel_tasks(bsp);

	arch_ipl_set(old);
}
