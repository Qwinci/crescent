#pragma once
#include "utility.hpp"

namespace __spinlock_detail { // NOLINT(*-reserved-identifier)
	template<typename T>
	struct Data {
		T value {};
		bool lock {};
	};
	template<>
	struct Data<void> {
		bool lock {};
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
#ifdef __aarch64__
			asm volatile("sev");
#endif
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
		while (true) {
			if (!__atomic_exchange_n(&data.lock, true, __ATOMIC_ACQUIRE)) {
				break;
			}
			while (__atomic_load_n(&data.lock, __ATOMIC_RELAXED)) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
		}
		return Guard {this};
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
#ifdef __aarch64__
			asm volatile("sev");
#endif
		}

	private:
		constexpr explicit Guard(Spinlock* owner) : owner {owner} {}

		friend class Spinlock;
		Spinlock* owner;
	};

	[[nodiscard]] Guard lock() {
		while (true) {
			if (!__atomic_exchange_n(&data.lock, true, __ATOMIC_ACQUIRE)) {
				break;
			}
			while (__atomic_load_n(&data.lock, __ATOMIC_RELAXED)) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
		}
		return Guard {this};
	}

private:
	__spinlock_detail::Data<void> data {};
};
