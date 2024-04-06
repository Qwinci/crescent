#pragma once
#include "new.hpp"

namespace std {
	template<typename T, typename... Args>
	constexpr T* construct_at(T* p, Args&&... args) {
		return new (static_cast<void*>(p)) T {static_cast<Args>(args)...};
	}
}
