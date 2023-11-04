#include "mutex.h"
#include "arch/cpu.h"
#include "arch/interrupts.h"

bool mutex_try_lock(Mutex* self) {
	return !atomic_exchange_explicit(&self->inner, true, memory_order_acquire);
}

void mutex_lock(Mutex* self) {
	bool result = atomic_exchange_explicit(&self->inner, true, memory_order_acquire);
	if (result) {
		Task* task = arch_get_cur_task();
		Ipl old = arch_ipl_set(IPL_CRITICAL);

		task->status = TASK_STATUS_WAITING;
		task->next = NULL;

		spinlock_lock(&self->task_protector);

		if (self->waiting_tasks) {
			self->waiting_tasks_end->next = task;
			self->waiting_tasks_end = task;
		}
		else {
			self->waiting_tasks = task;
			self->waiting_tasks_end = task;
		}

		spinlock_unlock(&self->task_protector);

		arch_ipl_set(old);
		sched();
	}
}

void mutex_unlock(Mutex* self) {
	atomic_store_explicit(&self->inner, false, memory_order_release);
	Ipl old = arch_ipl_set(IPL_CRITICAL);
	spinlock_lock(&self->task_protector);
	if (self->waiting_tasks) {
		Task* task = self->waiting_tasks;
		self->waiting_tasks = task->next;
		spinlock_unlock(&self->task_protector);
		arch_ipl_set(old);
		sched_unblock(task);
	}
	else {
		spinlock_unlock(&self->task_protector);
		arch_ipl_set(old);
	}
}

