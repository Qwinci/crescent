#pragma once
#include "cstddef.hpp"

constexpr void* operator new(size_t, void* ptr) {
	return ptr;
}
