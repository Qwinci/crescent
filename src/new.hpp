#pragma once
#include "types.hpp"

constexpr void* operator new(usize, void* ptr) {
	return ptr;
}