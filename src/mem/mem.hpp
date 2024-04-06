#pragma once
#include "types.hpp"

template<typename T>
inline T* to_virt(usize phys) {
	extern usize HHDM_START;
	return reinterpret_cast<T*>(phys + HHDM_START);
}

template<typename T>
inline usize to_phys(T* ptr) {
	extern usize HHDM_START;
	return reinterpret_cast<usize>(ptr) - HHDM_START;
}

#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))
#define ALIGNDOWN(value, align) ((value) & ~((align) - 1))
