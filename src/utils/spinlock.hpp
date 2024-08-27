#pragma once
#include "utility.hpp"

namespace __spinlock_detail { // NOLINT(*-reserved-identifier)
#ifdef __aarch64__
	using LockType = u32;
#else
	using LockType = bool;
#endif

	template<typename T>
	struct Data {
		T value {};
		LockType lock {};
	};
	template<>
	struct Data<void> {
		LockType lock {};
	};
}

template<typename T>
class Spinlock {
public:
	constexpr Spinlock() = default;
	constexpr explicit Spinlock(T&& data) : data {.value = std::move(data)} {}
	constexpr explicit Spinlock(const T& data) : data {data} {}

	class Guard {
	public:
		inline ~Guard() {
			__atomic_store_n(&owner->data.lock, false, __ATOMIC_RELEASE);
		}

		operator T&() { // NOLINT(*-explicit-constructor)
			return owner->data.value;
		}

		T& operator*() {
			return owner->data.value;
		}

		T* operator->() {
			return &owner->data.value;
		}

	private:
		constexpr explicit Guard(Spinlock* owner) : owner {owner} {}

		friend class Spinlock;
		Spinlock* owner;
	};

	[[nodiscard]] Guard lock() {
#ifdef __aarch64__
		__spinlock_detail::LockType value;
		__spinlock_detail::LockType one = 1;
		asm volatile(R"(
			sevl
			prfm pstl1keep, %0
			0:
			wfe
			ldaxr %w1, %0
			cbnz %w1, 0b
			stxr %w1, %w2, %0
			cbnz %w1, 0b
		)" : "+Q"(data.lock), "=&r"(value) : "r"(one) : "memory");
#else
		while (true) {
			if (!__atomic_exchange_n(&data.lock, true, __ATOMIC_ACQUIRE)) {
				break;
			}
			while (__atomic_load_n(&data.lock, __ATOMIC_RELAXED)) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#endif
			}
		}
#endif
		return Guard {this};
	}

	[[nodiscard]] inline bool is_locked() const {
		return __atomic_load_n(&data.lock, __ATOMIC_RELAXED);
	}

	T& get_unsafe() {
		return data.value;
	}

private:
	__spinlock_detail::Data<T> data {};
};

template<>
class Spinlock<void> {
public:
	constexpr Spinlock() = default;

	class Guard {
	public:
		inline ~Guard() {
			__atomic_store_n(&owner->data.lock, false, __ATOMIC_RELEASE);
		}

	private:
		constexpr explicit Guard(Spinlock* owner) : owner {owner} {}

		friend class Spinlock;
		Spinlock* owner;
	};

	[[nodiscard]] Guard lock() {
#ifdef __aarch64__
		__spinlock_detail::LockType value;
		__spinlock_detail::LockType one = 1;
		asm volatile(R"(
			sevl
			prfm pstl1keep, %0
			0:
			wfe
			ldaxr %w1, %0
			cbnz %w1, 0b
			stxr %w1, %w2, %0
			cbnz %w1, 0b
		)" : "+Q"(data.lock), "=&r"(value) : "r"(one) : "memory");
#else
		while (true) {
			if (!__atomic_exchange_n(&data.lock, true, __ATOMIC_ACQUIRE)) {
				break;
			}
			while (__atomic_load_n(&data.lock, __ATOMIC_RELAXED)) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#endif
			}
		}
#endif
		return Guard {this};
	}

	[[nodiscard]] inline bool is_locked() const {
		return __atomic_load_n(&data.lock, __ATOMIC_RELAXED);
	}

	void manual_lock() {
#ifdef __aarch64__
		__spinlock_detail::LockType value;
		__spinlock_detail::LockType one = 1;
		asm volatile(R"(
			sevl
			prfm pstl1keep, %0
			0:
			wfe
			ldaxr %w1, %0
			cbnz %w1, 0b
			stxr %w1, %w2, %0
			cbnz %w1, 0b
		)" : "+Q"(data.lock), "=&r"(value) : "r"(one) : "memory");
#else
		while (true) {
			if (!__atomic_exchange_n(&data.lock, true, __ATOMIC_ACQUIRE)) {
				break;
			}
			while (__atomic_load_n(&data.lock, __ATOMIC_RELAXED)) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#endif
			}
		}
#endif
	}

	void manual_unlock() {
		__atomic_store_n(&data.lock, false, __ATOMIC_RELEASE);
	}

private:
	__spinlock_detail::Data<void> data {};
};
