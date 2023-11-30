#pragma once
#include "utils/spinlock.h"
#include <stdatomic.h>

typedef struct Task Task;

typedef struct {
	Task* waiting_tasks;
	Task* waiting_tasks_end;
	Spinlock task_protector;
	atomic_bool inner;
} Mutex;

bool mutex_try_lock(Mutex* self);
void mutex_lock(Mutex* self);
void mutex_unlock(Mutex* self);