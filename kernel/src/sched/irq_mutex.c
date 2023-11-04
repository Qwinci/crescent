#include "irq_mutex.h"
#include "arch/cpu.h"
#include "arch/interrupts.h"

bool irq_mutex_try_lock(IrqMutex* self) {
	Ipl ipl = arch_ipl_set(IPL_CRITICAL);
	if (!atomic_exchange_explicit(&self->inner, true, memory_order_acquire)) {
		self->ipl = (u8) ipl;
		return true;
	}
	else {
		arch_ipl_set(ipl);
		return false;
	}
}

void irq_mutex_lock(IrqMutex* self) {
	Ipl ipl = arch_ipl_set(IPL_CRITICAL);
	bool result = atomic_exchange_explicit(&self->inner, true, memory_order_acquire);
	if (result) {
		Task* task = arch_get_cur_task();

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

		arch_ipl_set(ipl);
		sched();
		arch_ipl_set(IPL_CRITICAL);
	}
	self->ipl = (u8) ipl;
}

void irq_mutex_unlock(IrqMutex* self) {
	spinlock_lock(&self->task_protector);
	if (self->waiting_tasks) {
		Task* task = self->waiting_tasks;
		self->waiting_tasks = task->next;
		spinlock_unlock(&self->task_protector);

		atomic_store_explicit(&self->inner, false, memory_order_release);
		arch_ipl_set(self->ipl);

		if (sched_unblock(task)) {
			sched();
		}
	}
	else {
		spinlock_unlock(&self->task_protector);
		atomic_store_explicit(&self->inner, false, memory_order_release);
		arch_ipl_set(self->ipl);
	}
}

