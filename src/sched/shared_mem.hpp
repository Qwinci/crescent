#pragma once
#include "vector.hpp"

struct SharedMemory {
	~SharedMemory();

	kstd::vector<usize> pages;
	Spinlock<usize> usage_count;
};
