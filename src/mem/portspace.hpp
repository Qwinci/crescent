#pragma once
#include "types.hpp"
#include "x86/io.hpp"

struct PortSpace {
	constexpr explicit PortSpace(u32 base) : base {base} {}

	template<typename R>
	void store(R reg, typename R::bits_type value) {
		using type = typename R::bits_type;
		if constexpr (sizeof(type) == 1) {
			x86::out1(base + reg.offset, value);
		}
		else if constexpr (sizeof(type) == 2) {
			x86::out2(base + reg.offset, value);
		}
		else if constexpr (sizeof(type) == 4) {
			x86::out4(base + reg.offset, value);
		}
		else {
			static_assert(false);
		}
	}

	template<typename T>
	void store(u32 offset, T value) {
		if constexpr (sizeof(T) == 1) {
			x86::out1(base + offset, value);
		}
		else if constexpr (sizeof(T) == 2) {
			x86::out2(base + offset, value);
		}
		else if constexpr (sizeof(T) == 4) {
			x86::out4(base + offset, value);
		}
		else {
			static_assert(false);
		}
	}

	template<typename R>
	typename R::type load(R reg) {
		using bits_type = typename R::bits_type;
		using type = typename R::type;
		if constexpr (sizeof(bits_type) == 1) {
			return static_cast<type>(x86::in1(base + reg.offset));
		}
		else if constexpr (sizeof(bits_type) == 2) {
			return static_cast<type>(x86::in2(base + reg.offset));
		}
		else if constexpr (sizeof(bits_type) == 4) {
			return static_cast<type>(x86::in4(base + reg.offset));
		}
		else {
			static_assert(false);
		}
	}

	template<typename T>
	T load(uintptr_t offset) {
		if constexpr (sizeof(T) == 1) {
			return static_cast<T>(x86::in1(base + offset));
		}
		else if constexpr (sizeof(T) == 2) {
			return static_cast<T>(x86::in2(base + offset));
		}
		else if constexpr (sizeof(T) == 4) {
			return static_cast<T>(x86::in4(base + offset));
		}
		else {
			static_assert(false);
		}
	}

private:
	u32 base;
};
