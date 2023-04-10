#include "mutex.h"
#include "arch/cpu.h"
#include "arch/misc.h"

bool mutex_try_lock(Mutex* self) {
	return !atomic_exchange_explicit(&self->inner, true, memory_order_acquire);
}

void mutex_lock(Mutex* self) {
	bool result = atomic_exchange_explicit(&self->inner, true, memory_order_acquire);
	if (result) {
		Task* task = arch_get_cur_task();
		void* flags = enter_critical();

		task->status = TASK_STATUS_WAITING;
		task->next = NULL;

		spinlock_lock(&self->task_protector);

		if (self->waiting_tasks) {
			self->waiting_tasks_end->next = task;
			self->waiting_tasks_end = task;
		}
		else {
			self->waiting_tasks = task;
		}

		spinlock_unlock(&self->task_protector);
		leave_critical(flags);
		sched();
	}
}

void mutex_unlock(Mutex* self) {
	atomic_store_explicit(&self->inner, false, memory_order_release);
	void* flags = enter_critical();
	spinlock_lock(&self->task_protector);
	if (self->waiting_tasks) {
		Task* task = self->waiting_tasks;
		self->waiting_tasks = task->next;
		spinlock_unlock(&self->task_protector);
		leave_critical(flags);

		if (sched_unblock(task)) {
			sched();
		}
	}
	else {
		spinlock_unlock(&self->task_protector);
		leave_critical(flags);
	}
}