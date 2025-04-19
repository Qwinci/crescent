#pragma once
#include "types.hpp"

template<typename T, typename R>
struct Register {
	constexpr explicit Register(usize offset) : offset {offset} {}

	using bits_type = T;
	using type = R;
	usize offset;
};

template<typename T>
struct BitValue {
	using type = T;

	constexpr operator type() const volatile { // NOLINT(*-explicit-constructor)
		return value;
	}

	constexpr operator type() const { // NOLINT(*-explicit-constructor)
		return value;
	}

	constexpr void operator|=(type bits) volatile {
		value |= bits;
	}
	constexpr void operator&=(type bits) volatile {
		value &= bits;
	}

	constexpr BitValue& operator|=(type bits) {
		value |= bits;
		return *this;
	}
	constexpr BitValue& operator&=(type bits) {
		value &= bits;
		return *this;
	}

	T value;
};

template<typename B, typename T>
struct BitField {
	friend constexpr T operator&(BitValue<B> value, BitField field) {
		return static_cast<T>((value.value & field.mask) >> field.offset);
	}

	constexpr BitField(int offset, int bits) : offset {offset} {
		mask = ((B {1} << bits) - 1) << offset;
	}

	constexpr B operator()(T value) const {
		return static_cast<B>(value) << offset;
	}

	constexpr B operator~() const {
		return ~mask;
	}

	B mask;
	int offset;
};

struct MemSpace {
	constexpr explicit MemSpace(usize base) : base {base} {}

	template<typename R>
	void store(R reg, typename R::bits_type value) {
		auto ptr = reinterpret_cast<volatile typename R::bits_type*>(base + reg.offset);
		ops::store(ptr, value);
	}

	template<typename T>
	void store(usize offset, T value) {
		auto ptr = reinterpret_cast<volatile T*>(base + offset);
		ops::store(ptr, value);
	}

	template<typename R>
	typename R::type load(R reg) {
		auto ptr = reinterpret_cast<const volatile typename R::bits_type*>(base + reg.offset);
		return static_cast<typename R::type>(ops::load(ptr));
	}

	template<typename T>
	T load(usize offset) {
		auto ptr = reinterpret_cast<const volatile T*>(base + offset);
		return ops::load(ptr);
	}

protected:
	usize base;

private:
#ifdef __x86_64__
	struct ops {
		[[gnu::always_inline]] static inline void store(volatile u8* ptr, u8 value) {
			asm volatile("movb %1, %0" : "=m"(*ptr) : "er"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u16* ptr, u16 value) {
			asm volatile("movw %1, %0" : "=m"(*ptr) : "er"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u32* ptr, u32 value) {
			asm volatile("movl %1, %0" : "=m"(*ptr) : "er"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u64* ptr, u64 value) {
			asm volatile("movq %1, %0" : "=m"(*ptr) : "er"(value));
		}

		[[gnu::always_inline]] static inline u8 load(const volatile u8* ptr) {
			u8 value;
			asm volatile("movb %1, %0" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u16 load(const volatile u16* ptr) {
			u16 value;
			asm volatile("movw %1, %0" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u32 load(const volatile u32* ptr) {
			u32 value;
			asm volatile("movl %1, %0" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u64 load(const volatile u64* ptr) {
			u64 value;
			asm volatile("movq %1, %0" : "=r"(value) : "m"(*ptr));
			return value;
		}
	};
#elif defined(__aarch64__)
	struct ops {
		[[gnu::always_inline]] static inline void store(volatile u8* ptr, u8 value) {
			asm volatile("strb %w1, %0" : "=m"(*ptr) : "r"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u16* ptr, u16 value) {
			asm volatile("strh %w1, %0" : "=m"(*ptr) : "r"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u32* ptr, u32 value) {
			asm volatile("str %w1, %0" : "=m"(*ptr) : "r"(value));
		}

		[[gnu::always_inline]] static inline void store(volatile u64* ptr, u64 value) {
			asm volatile("str %1, %0" : "=m"(*ptr) : "r"(value));
		}

		[[gnu::always_inline]] static inline u8 load(const volatile u8* ptr) {
			u8 value;
			asm volatile("ldrb %w0, %1" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u16 load(const volatile u16* ptr) {
			u16 value;
			asm volatile("ldrh %w0, %1" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u32 load(const volatile u32* ptr) {
			u32 value;
			asm volatile("ldr %w0, %1" : "=r"(value) : "m"(*ptr));
			return value;
		}

		[[gnu::always_inline]] static inline u64 load(const volatile u64* ptr) {
			u64 value;
			asm volatile("ldr %0, %1" : "=r"(value) : "m"(*ptr));
			return value;
		}
	};
#endif
};

template<typename T>
using BasicRegister = Register<T, T>;
template<typename T>
using BitRegister = Register<T, BitValue<T>>;
