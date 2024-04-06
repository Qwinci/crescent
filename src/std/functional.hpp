#pragma once
#include "memory.hpp"
#include "utility.hpp"

namespace kstd {
	template<typename R, typename... Args>
	struct small_function;

	template<typename R, typename... Args>
	struct small_function<R(Args...)> {
		static constexpr size_t SIZE = sizeof(void (*)()) * 2;

		constexpr small_function() = default;

		template<typename F>
		small_function(F&& fn) // NOLINT(*-explicit-constructor)
			requires (!is_same_v<remove_reference_t<F>, small_function> && requires(F f, Args... args) {
			f(std::forward<Args>(args)...);
		}) {
			using type = kstd::remove_reference_t<F>;
			static_assert(sizeof(type) <= SIZE);
			new (storage) type {fn};
			invoke = &small_function::specific_invoke<F>;
			destroy = &small_function::specific_destroy<F>;
			move_to = &small_function::specific_move<F>;
		}

		small_function(R (*fn)(Args...)) { // NOLINT(*-explicit-constructor)
			new (storage) (R (*)(Args...)) {fn};
			invoke = &small_function::specific_invoke<R (*)(Args...)>;
			destroy = &small_function::specific_destroy<R (*)(Args...)>;
			move_to = &small_function::specific_move<R (*)(Args...)>;
		}

		~small_function() {
			if (destroy) {
				(this->*destroy)();
			}
		}

		constexpr small_function(small_function&& other) {
			((&other)->*(other.move_to))(*this);
		}

		constexpr small_function(const small_function&) = delete;

		constexpr small_function& operator=(small_function&& other) {
			if (destroy) {
				(this->*destroy)();
			}
			((&other)->*(other.move_to))(*this);
			return *this;
		}

		R operator()(Args... args) {
			return (this->*invoke)(std::forward<Args>(args)...);
		}

		constexpr explicit operator bool() const {
			return destroy;
		}

	private:
		template<typename F>
		R specific_invoke(Args... args) {
			return (*static_cast<kstd::remove_reference_t<F>*>(static_cast<void*>(storage)))(std::forward<Args>(args)...);
		}

		template<typename F>
		void specific_destroy() {
			using type = kstd::remove_reference_t<F>;
			(*static_cast<type*>(static_cast<void*>(storage))).~type();
		}

		template<typename F>
		void specific_move(small_function& other) {
			using type = kstd::remove_reference_t<F>;
			new (other.storage) type {std::move(*static_cast<type*>(static_cast<void*>(storage)))};
			other.invoke = invoke;
			other.destroy = destroy;
			other.move_to = move_to;
			destroy = nullptr;
		}

		R (small_function::* invoke)(Args... args);
		void (small_function::* destroy)();
		void (small_function::* move_to)(small_function& other);
		alignas(sizeof(void (*)()) * 2) char storage[SIZE] {};
	};

	template<typename R, typename... Args>
	small_function(R (*)(Args...)) -> small_function<R(Args...)>;
}
