#pragma once
#include "utility.hpp"
#include "assert.hpp"

namespace kstd {
	template<typename T>
	class shared_ptr {
	public:
		constexpr shared_ptr() = default;
		constexpr shared_ptr(kstd::nullptr_t) : ptr {nullptr}, refs {nullptr} {} // NOLINT(*-explicit-constructor)

		template<typename U = T>
		explicit shared_ptr(U&& value) requires(!is_same_v<remove_reference_t<U>, shared_ptr>) { // NOLINT(*-explicit-constructor)
			ptr = new U {std::forward<U>(value)};
			refs = new size_t {1};
		}
		template<typename U = T>
		explicit shared_ptr(const U& value) {
			ptr = new U {value};
			refs = new size_t {1};
		}

		template<typename U> requires(__is_base_of(U, T))
		constexpr operator shared_ptr<U>() { // NOLINT(*-explicit-constructor)
			shared_ptr<U> new_ptr {};
			new_ptr.ptr = ptr;
			new_ptr.refs = refs;
			if (ptr) {
				++*refs;
			}
			return new_ptr;
		}

		shared_ptr(shared_ptr&& other) { // NOLINT(*-explicit-constructor)
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
		}
		shared_ptr(const shared_ptr& other) { // NOLINT(*-explicit-constructor)
			ptr = other.ptr;
			refs = other.refs;
			if (ptr) {
				++*refs;
			}
		}

		shared_ptr& operator=(shared_ptr&& other) {
			assert(this != &other);

			if (ptr && --*refs == 0) {
				delete ptr;
			}
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
			return *this;
		}
		shared_ptr& operator=(const shared_ptr& other) {
			assert(this != &other);

			if (ptr && --*refs == 0) {
				delete ptr;
			}
			ptr = other.ptr;
			refs = other.refs;
			if (ptr) {
				++*refs;
			}

			return *this;
		}

		constexpr T* data() const {
			return ptr;
		}

		constexpr operator T*() { // NOLINT(*-explicit-constructor)
			return ptr;
		}

		constexpr operator const T*() const { // NOLINT(*-explicit-constructor)
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

		~shared_ptr() {
			if (ptr && --*refs == 0) {
				delete ptr;
				delete refs;
			}
		}

		T* ptr {};
		size_t* refs {};
	};

	template<typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args) {
		return shared_ptr<T> {T {std::forward<Args&&>(args)...}};
	}
}
