#pragma once
#include "utility.hpp"
#include "assert.hpp"

namespace kstd {
	template<typename T>
	class shared_ptr {
	public:
		constexpr shared_ptr() = default;
		constexpr shared_ptr(kstd::nullptr_t) : ptr {nullptr}, refs {nullptr} {} // NOLINT(*-explicit-constructor)

		constexpr explicit shared_ptr(T* ptr) : ptr {ptr} {
			refs = new size_t {1};
		}

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
				++*refs;
			}
		}

		template<typename U>
		constexpr shared_ptr(shared_ptr<U>&& other) { // NOLINT(*-explicit-constructor)
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
		}
		template<typename U>
		shared_ptr(const shared_ptr<U>& other) { // NOLINT(*-explicit-constructor)
			ptr = other.ptr;
			refs = other.refs;
			if (ptr) {
				++*refs;
			}
		}

		template<typename U = T>
		shared_ptr& operator=(shared_ptr<U>&& other) {
			if (ptr && --*refs == 0) {
				delete ptr;
			}
			ptr = other.ptr;
			refs = other.refs;
			other.ptr = nullptr;
			other.refs = nullptr;
			return *this;
		}
		template<typename U = T>
		shared_ptr& operator=(const shared_ptr<U>& other) {
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
			if (ptr && --*refs == 0) {
				delete ptr;
				delete refs;
			}
		}

	private:
		template<typename>
		friend class shared_ptr;

		T* ptr {};
		size_t* refs {};
	};

	template<typename T, typename... Args>
	shared_ptr<T> make_shared(Args&&... args) {
		return shared_ptr<T> {new T {std::forward<Args&&>(args)...}};
	}
}
