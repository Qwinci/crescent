#pragma once
#include "types.hpp"
#include "optional.hpp"

struct UniquePhysical {
	constexpr UniquePhysical(usize base, usize count) : base {base}, count {count} {}

	static kstd::optional<UniquePhysical> create(usize count);

	constexpr UniquePhysical(UniquePhysical&& other) {
		base = other.base;
		count = other.count;
		other.base = 0;
		other.count = 0;
	}

	UniquePhysical(const UniquePhysical&) = delete;
	constexpr UniquePhysical& operator=(const UniquePhysical&) = delete;

	UniquePhysical& operator=(UniquePhysical&& other);

	~UniquePhysical();

	usize base;
	usize count;
};
