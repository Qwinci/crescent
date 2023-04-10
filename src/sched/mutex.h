#pragma once
#include "utils/spinlock.h"
#include <stdatomic.h>

typedef struct Task Task;

typedef struct {
	atomic_bool inner;
	Spinlock task_protector;
	Task* waiting_tasks;
	Task* waiting_tasks_end;
} Mutex;

bool mutex_try_lock(Mutex* self);
void mutex_lock(Mutex* self);
void mutex_unlock(Mutex* self);