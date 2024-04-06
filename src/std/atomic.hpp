#pragma once
#include "type_traits.hpp"
#include "cstdint.hpp"
#include "cstddef.hpp"

namespace kstd {
	enum class memory_order : int {
		relaxed = __ATOMIC_RELAXED,
		consume = __ATOMIC_CONSUME,
		acquire = __ATOMIC_ACQUIRE,
		release = __ATOMIC_RELEASE,
		acq_rel = __ATOMIC_ACQ_REL,
		seq_cst = __ATOMIC_SEQ_CST
	};

	template<typename T>
	struct atomic {
		using value_type = T;

		constexpr atomic() = default;
		constexpr atomic(T desired) : value {desired} {}
		constexpr atomic(const atomic&) = delete;

		constexpr atomic& operator=(const atomic&) = delete;

		void store(T desired, memory_order order) {
			__atomic_store_n(&value, desired, static_cast<int>(order));
		}

		T load(memory_order order) const {
			return __atomic_load_n(&value, static_cast<int>(order));
		}

		T exchange(T desired, memory_order order) {
			return __atomic_exchange_n(&value, desired, static_cast<int>(order));
		}

		bool compare_exchange_weak(T& expected, T desired, memory_order success, memory_order failure) {
			return __atomic_compare_exchange_n(&value, &expected, desired, true, static_cast<int>(success), static_cast<int>(failure));
		}

		bool compare_exchange_weak(T& expected, T desired, memory_order order) {
			return __atomic_compare_exchange_n(&value, &expected, desired, true, static_cast<int>(order), static_cast<int>(order));
		}

		bool compare_exchange_strong(T& expected, T desired, memory_order success, memory_order failure) {
			return __atomic_compare_exchange_n(&value, &expected, desired, false, static_cast<int>(success), static_cast<int>(failure));
		}

		bool compare_exchange_strong(T&& expected, T desired, memory_order success, memory_order failure) {
			return __atomic_compare_exchange_n(&value, &expected, desired, false, static_cast<int>(success), static_cast<int>(failure));
		}

		bool compare_exchange_strong(T& expected, T desired, memory_order order) {
			return __atomic_compare_exchange_n(&value, &expected, desired, false, static_cast<int>(order), static_cast<int>(order));
		}

		bool compare_exchange_strong(T&& expected, T desired, memory_order order) {
			return __atomic_compare_exchange_n(&value, &expected, desired, false, static_cast<int>(order), static_cast<int>(order));
		}

		T fetch_add(T arg, memory_order order) requires(is_integral_v<T>) {
			return __atomic_fetch_add(&value, arg, static_cast<int>(order));
		}
		T fetch_add(ptrdiff_t arg, memory_order order) requires(is_pointer_v<T>) {
			return __atomic_fetch_add(&value, arg, static_cast<int>(order));
		}

		T fetch_sub(T arg, memory_order order) requires(is_integral_v<T>) {
			return __atomic_fetch_sub(&value, arg, static_cast<int>(order));
		}
		T fetch_sub(ptrdiff_t arg, memory_order order) requires(is_pointer_v<T>) {
			return __atomic_fetch_sub(&value, arg, static_cast<int>(order));
		}

		T fetch_and(T arg, memory_order order) requires(is_integral_v<T>) {
			return __atomic_fetch_and(&value, arg, static_cast<int>(order));
		}
		T fetch_or(T arg, memory_order order) requires(is_integral_v<T>) {
			return __atomic_fetch_or(&value, arg, static_cast<int>(order));
		}
		T fetch_xor(T arg, memory_order order) requires(is_integral_v<T>) {
			return __atomic_fetch_xor(&value, arg, static_cast<int>(order));
		}

	private:
		T value;
	};
}
