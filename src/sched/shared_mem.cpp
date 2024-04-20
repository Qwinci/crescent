#include "shared_mem.hpp"
#include "mem/pmalloc.hpp"

SharedMemory::~SharedMemory() {
	auto guard = usage_count.lock();
	--*guard;
	if (!*guard) {
		for (auto page : pages) {
			pfree(page, 1);
		}
	}
}
