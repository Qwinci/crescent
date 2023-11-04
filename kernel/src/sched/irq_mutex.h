#pragma once
#include "utils/spinlock.h"
#include "types.h"
#include <stdatomic.h>

typedef struct Task Task;

typedef struct {
	Task* waiting_tasks;
	Task* waiting_tasks_end;
	Spinlock task_protector;
	atomic_bool inner;
	u8 ipl;
} IrqMutex;

bool irq_mutex_try_lock(IrqMutex* self);
void irq_mutex_lock(IrqMutex* self);
void irq_mutex_unlock(IrqMutex* self);