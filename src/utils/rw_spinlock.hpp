#pragma once
#include "utility.hpp"
#include "atomic.hpp"

namespace __rwspinlock_detail { // NOLINT(*-reserved-identifier)
	template<typename T>
	struct Data {
		T value {};
		kstd::atomic<int> lock {};
	};
	template<>
	struct Data<void> {
		kstd::atomic<int> lock {};
	};
}

template<typename T>
class RwSpinlock {
public:
	constexpr RwSpinlock() = default;
	constexpr explicit RwSpinlock(T&& data) : data {.value = std::move(data)} {}
	constexpr explicit RwSpinlock(const T& data) : data {data} {}

	class ReadGuard {
	public:
		inline ~ReadGuard() {
			owner->data.lock.fetch_sub(-1, kstd::memory_order::release);
#ifdef __aarch64__
			asm volatile("sev");
#endif
		}

		operator const T&() { // NOLINT(*-explicit-constructor)
			return owner->data.value;
		}

		const T& operator*() {
			return owner->data.value;
		}

		const T* operator->() {
			return &owner->data.value;
		}

	private:
		constexpr explicit ReadGuard(RwSpinlock* owner) : owner {owner} {}

		friend class RwSpinlock;
		RwSpinlock* owner;
	};

	class WriteGuard {
	public:
		inline ~WriteGuard() {
			owner->data.lock.store(0, kstd::memory_order::release);
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
		constexpr explicit WriteGuard(RwSpinlock* owner) : owner {owner} {}

		friend class RwSpinlock;
		RwSpinlock* owner;
	};

	[[nodiscard]] ReadGuard lock_read() {
		while (true) {
			int users;
			while ((users = data.lock.load(kstd::memory_order::acquire)) == -1) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
			if (data.lock.compare_exchange_strong(users, users + 1, kstd::memory_order::acquire, kstd::memory_order::acquire)) {
				break;
			}
		}
		return ReadGuard {this};
	}

	[[nodiscard]] WriteGuard lock_write() {
		while (true) {
			int users;
			while ((users = data.lock.load(kstd::memory_order::acquire)) > 0) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
			if (data.lock.compare_exchange_strong(0, -1, kstd::memory_order::acquire, kstd::memory_order::acquire)) {
				break;
			}
		}
		return WriteGuard {this};
	}

private:
	__rwspinlock_detail::Data<T> data {};
};

template<>
class RwSpinlock<void> {
public:
	constexpr RwSpinlock() = default;

	class ReadGuard {
	public:
		inline ~ReadGuard() {
			owner->data.lock.fetch_sub(-1, kstd::memory_order::release);
#ifdef __aarch64__
			asm volatile("sev");
#endif
		}

	private:
		constexpr explicit ReadGuard(RwSpinlock* owner) : owner {owner} {}

		friend class RwSpinlock;
		RwSpinlock* owner;
	};

	class WriteGuard {
	public:
		inline ~WriteGuard() {
			owner->data.lock.store(0, kstd::memory_order::release);
#ifdef __aarch64__
			asm volatile("sev");
#endif
		}

	private:
		constexpr explicit WriteGuard(RwSpinlock* owner) : owner {owner} {}

		friend class RwSpinlock;
		RwSpinlock* owner;
	};

	[[nodiscard]] ReadGuard lock_read() {
		while (true) {
			int users;
			while ((users = data.lock.load(kstd::memory_order::acquire)) == -1) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
			if (data.lock.compare_exchange_strong(users, users + 1, kstd::memory_order::acquire, kstd::memory_order::acquire)) {
				break;
			}
		}
		return ReadGuard {this};
	}

	[[nodiscard]] WriteGuard lock_write() {
		while (true) {
			int users;
			while ((users = data.lock.load(kstd::memory_order::acquire)) > 0) {
#if defined(__x86_64__)
				__builtin_ia32_pause();
#elif defined(__aarch64__)
				asm volatile("wfe");
#endif
			}
			if (data.lock.compare_exchange_strong(0, -1, kstd::memory_order::acquire, kstd::memory_order::acquire)) {
				break;
			}
		}
		return WriteGuard {this};
	}

private:
	__rwspinlock_detail::Data<void> data {};
};
