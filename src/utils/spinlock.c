#include "spinlock.h"
#include "arch/misc.h"

void spinlock_lock(Spinlock* self) {
	while (true) {
		if (!atomic_exchange_explicit(&self->lock, true, memory_order_acquire)) {
			break;
		}
		while (atomic_load_explicit(&self->lock, memory_order_relaxed)) {
			arch_spinloop_hint();
		}
	}
}

bool spinlock_try_lock(Spinlock* self) {
	return !atomic_exchange_explicit(&self->lock, true, memory_order_acquire);
}

void spinlock_unlock(Spinlock* self) {
	atomic_store_explicit(&self->lock, false, memory_order_release);
}