#pragma once
#include "types.hpp"
#include "arch/cpu.hpp"

extern char __CPU_LOCAL_START[];

template<typename T>
struct CpuVar {
	struct Guard {
		~Guard() {
			arch_enable_irqs(old);
		}

		T& get() {
			return *ptr;
		}

		T* operator->() {
			return ptr;
		}

		T& operator*() {
			return *ptr;
		}

	private:
		friend CpuVar;
		constexpr explicit Guard(T* ptr, bool old) : ptr {ptr}, old {old} {}
		T* ptr;
		bool old;
	};

	Guard get() {
		bool old = arch_enable_irqs(false);
		auto ptr = __get_unsafe();
		return Guard {ptr, old};
	}

	T* __get_unsafe() {
		usize base;
#ifdef __x86_64__
		asm volatile("mov %%gs:160, %0" : "=r"(base));
#elif defined(__aarch64__)
		usize thread;
		asm volatile("mrs %0, tpidr_el1" : "=r"(thread));
		base = *reinterpret_cast<usize*>(thread + 144);
#else
#error missing architecture specific code
#endif
		return __builtin_launder(reinterpret_cast<T*>(base + __offset));
	}

	usize __offset;
};

#define per_cpu(type, name, ctor, modifiers...) \
	[[gnu::section(".bss.cpu_local")]] alignas(type) modifiers char __cpu_local_ ## name[sizeof(type)]; \
	CpuVar<type> name {sizeof(Cpu) + reinterpret_cast<uintptr_t>(&__cpu_local_ ##name) - reinterpret_cast<uintptr_t>(__CPU_LOCAL_START)}; \
	[[gnu::section(".cpu_local_ctors"), gnu::used]] void (*__cpu_local_ ## name ## _ctor)() = []() { ctor(name.__get_unsafe()); }
#define per_cpu_trivial(type, name, value, modifiers...) \
	static_assert(__is_trivially_constructible(type)); \
	per_cpu(type, name, [](void* ptr) { *__builtin_launder(static_cast<type*>(ptr)) = value; }, modifiers)
