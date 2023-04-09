#pragma once
#include <stdatomic.h>

typedef struct {
	atomic_bool lock;
} Spinlock;

void spinlock_lock(Spinlock* self);
bool spinlock_try_lock(Spinlock* self);
void spinlock_unlock(Spinlock* self);