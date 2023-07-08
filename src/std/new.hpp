#pragma once

constexpr void* operator new(size_t, void* ptr) {
	return ptr;
}

