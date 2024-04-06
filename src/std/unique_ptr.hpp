#pragma once
#include "utility.hpp"
#include "assert.hpp"

namespace kstd {
	template<typename T>
	class unique_ptr {
	public:
		constexpr unique_ptr() = default;
		constexpr unique_ptr(kstd::nullptr_t) : ptr {nullptr} {} // NOLINT(*-explicit-constructor)

		template<typename U = T>
		unique_ptr(U&& value) requires(!is_same_v<remove_reference_t<U>, unique_ptr>) { // NOLINT(*-explicit-constructor)
			ptr = new U {move(value)};
		}

		unique_ptr(unique_ptr&& other) { // NOLINT(*-explicit-constructor)
			ptr = other.ptr;
			other.ptr = nullptr;
		}
		unique_ptr(const unique_ptr&) = delete;

		unique_ptr& operator=(unique_ptr&& other) {
			ptr = other.ptr;
			return *this;
		}

		constexpr unique_ptr& operator=(const unique_ptr&) = delete;

		constexpr T* data() const {
			return ptr;
		}

		constexpr operator T*() { // NOLINT(*-explicit-constructor)
			return &ptr;
		}

		constexpr operator const T*() const { // NOLINT(*-explicit-constructor)
			return &ptr;
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

		~unique_ptr() {
			delete ptr;
		}

	private:
		T* ptr {};
	};
}
