#include "spinlock.h"
#include "arch/misc.h"

#ifdef CONFIG_TEST
#include "utils/test.h"
static bool test_spinlock_lock() {
	Spinlock lock = {};
	spinlock_lock(&lock);
	return !spinlock_try_lock(&lock);
}

static bool test_spinlock_unlock() {
	Spinlock lock = {};
	spinlock_lock(&lock);
	spinlock_unlock(&lock);
	return spinlock_try_lock(&lock);
}

TEST(spinlock_lock, test_spinlock_lock);
TEST(spinlock_unlock, test_spinlock_unlock);
#endif

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