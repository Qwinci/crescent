#pragma once
#include "types.hpp"

extern "C" bool mem_copy_to_user(usize user, const void* kernel, usize size);
extern "C" bool mem_copy_to_kernel(void* kernel, usize user, usize size);
extern "C" bool atomic_load32_user(usize user, u32* ret);

struct UserAccessor {
	inline constexpr explicit UserAccessor(usize addr) : addr {addr} {}
	template<typename T>
	inline explicit UserAccessor(T* ptr) : addr {reinterpret_cast<usize>(ptr)} {}

	inline bool load(void* data, usize size) {
		return mem_copy_to_kernel(data, addr, size);
	}

	template<typename T>
	inline bool load(T& data) {
		return load(static_cast<void*>(&data), sizeof(T));
	}

	inline bool store(const void* data, usize size) {
		return mem_copy_to_user(addr, data, size);
	}

	template<typename T>
	inline bool store(const T& data) {
		return store(static_cast<const void*>(&data), sizeof(T));
	}

private:
	usize addr;
};
