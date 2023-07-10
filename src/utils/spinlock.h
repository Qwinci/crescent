#pragma once
#include <stdatomic.h>

typedef struct {
	atomic_bool lock;
} Spinlock;

typedef struct {
	atomic_uint_fast8_t lock;
} IrqSpinlock;

void spinlock_lock(Spinlock* self);
bool spinlock_try_lock(Spinlock* self);
void spinlock_unlock(Spinlock* self);
