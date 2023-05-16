#include "timer.h"

bool try_repeat_with_timeout(bool (*fn)(void* arg), void* arg, usize us) {
	usize start = arch_get_ns_since_boot();
	while (arch_get_ns_since_boot() < start + us * NS_IN_US) {
		if (fn(arg)) {
			return true;
		}
	}
	return false;
}