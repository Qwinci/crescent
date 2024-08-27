#include "unique_phys.hpp"
#include "pmalloc.hpp"

UniquePhysical& UniquePhysical::operator=(UniquePhysical&& other) {
	if (base) {
		pfree(base, count);
	}
	base = other.base;
	count = other.count;
	other.base = 0;
	other.count = 0;
	return *this;
}

UniquePhysical::~UniquePhysical() {
	if (base) {
		pfree(base, count);
	}
}

kstd::optional<UniquePhysical> UniquePhysical::create(usize count) {
	auto base = pmalloc(count);
	if (!base) {
		return {};
	}
	return UniquePhysical {base, count};
}
