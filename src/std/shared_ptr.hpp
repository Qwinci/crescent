#pragma once
#include "utility.hpp"
#include "assert.hpp"
#include "atomic.hpp"
#include "mem/malloc.hpp"
#include "new.hpp"

namespace kstd {
	template<typename T>
	class shared_ptr {
	public:
		constexpr shared_ptr() = default;
		constexpr shared_ptr(kstd::nullptr_t) : ptr {nullptr}, refs {nullptr} {} // NOLINT(*-explicit-constructor)

		constexpr shared_ptr(shared_ptr&& other) {
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
		}

		constexpr shared_ptr(const shared_ptr& other) {
			ptr = other.ptr;
			refs = other.refs;
			if (ptr) {
				increase_ref_count();
			}
		}

		template<typename U>
		constexpr shared_ptr(shared_ptr<U>&& other) { // NOLINT(*-explicit-constructor)
			ptr = static_cast<T*>(other.ptr);
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
		}
		template<typename U>
		shared_ptr(const shared_ptr<U>& other) { // NOLINT(*-explicit-constructor)
			ptr = static_cast<T*>(other.ptr);
			refs = other.refs;
			if (ptr) {
				increase_ref_count();
			}
		}

		template<typename U = T>
		shared_ptr& operator=(shared_ptr<U>&& other) {
			if (ptr && decrease_ref_count()) {
				delete ptr;
				delete refs;
			}
			ptr = static_cast<T*>(other.ptr);
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
			return *this;
		}
		template<typename U = T>
		shared_ptr& operator=(const shared_ptr<U>& other) {
			if (ptr && decrease_ref_count()) {
				delete ptr;
				delete refs;
			}
			ptr = static_cast<T*>(other.ptr);
			refs = other.refs;
			if (ptr) {
				increase_ref_count();
			}

			return *this;
		}

		constexpr T* data() {
			return ptr;
		}

		constexpr const T* data() const {
			return ptr;
		}

		constexpr T& operator*() {
			assert(ptr);
			return *ptr;
		}

		constexpr const T& operator*() const {
			assert(ptr);
			return *ptr;
		}

		constexpr T* operator->() {
			return ptr;
		}

		constexpr const T* operator->() const {
			return ptr;
		}

		constexpr explicit operator bool() const {
			return ptr;
		}

		~shared_ptr() {
			if (ptr && decrease_ref_count()) {
				delete ptr;
				delete refs;
			}
		}

	private:
		template<typename T2, typename... Args>
		friend shared_ptr<T2> make_shared(Args&&... args);

		constexpr shared_ptr(T* ptr, kstd::atomic<size_t>* refs) : ptr {ptr}, refs {refs} {}

		constexpr void increase_ref_count() {
			auto old = refs->load(kstd::memory_order::relaxed);
			while (!refs->compare_exchange_strong(
				old,
				old + 1,
				kstd::memory_order::acq_rel,
				kstd::memory_order::relaxed));
		}

		constexpr bool decrease_ref_count() {
			auto old = refs->fetch_sub(1, kstd::memory_order::acq_rel);
			return old == 1;
		}

		template<typename>
		friend class shared_ptr;

		T* ptr {};
		kstd::atomic<size_t>* refs {};
	};

	template<typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args) {
		auto* ptr = new T {std::forward<Args&&>(args)...};
		auto* refs = new kstd::atomic<size_t> {1};
		return shared_ptr<T> {ptr, refs};
	}
}
