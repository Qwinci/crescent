#pragma once

enum class MemoryOrder : int {
	Relaxed = __ATOMIC_RELAXED,
	Consume = __ATOMIC_CONSUME,
	Acquire = __ATOMIC_ACQUIRE,
	Release = __ATOMIC_RELEASE,
	AcqRel = __ATOMIC_ACQ_REL,
	SeqCst = __ATOMIC_SEQ_CST
};

template<typename T>
class Atomic {
public:
	constexpr Atomic() = default;
	constexpr explicit Atomic(T value) : value {value} {}

	void store(T desired, MemoryOrder order) volatile {
		__atomic_store_n(&value, desired, static_cast<int>(order));
	}

	T load(MemoryOrder order) const volatile {
		return __atomic_load_n(&value, static_cast<int>(order));
	}

	T exchange(T desired, MemoryOrder order) volatile {
		return __atomic_exchange_n(&value, desired, static_cast<int>(order));
	}

	bool compare_exchange_weak(T& expected, T desired, MemoryOrder success, MemoryOrder failure) volatile {
		return __atomic_compare_exchange_n(&value, &expected, desired, true, static_cast<int>(success), static_cast<int>(failure));
	}

	bool compare_exchange_strong(T& expected, T desired, MemoryOrder success, MemoryOrder failure) volatile {
		return __atomic_compare_exchange_n(&value, &expected, desired, false, static_cast<int>(success), static_cast<int>(failure));
	}
private:
	T value;
};