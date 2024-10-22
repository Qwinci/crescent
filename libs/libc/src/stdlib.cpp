#include "stdlib.h"
#include "stdio.h"
#include "sys.hpp"
#include "string.h"
#include <hz/slab.hpp>

namespace {
	struct ArenaAllocator {
		static void* allocate(size_t size) {
			size = (size + 0x1000 - 1) & ~(0x1000 - 1);
			void* ret = nullptr;
			if (sys_map(&ret, size, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE)) {
				return nullptr;
			}
			return ret;
		}

		static void deallocate(void* ptr, size_t size) {
			size = (size + 0x1000 - 1) & ~(0x1000 - 1);
			sys_unmap(ptr, size);
		}
	};

	constinit hz::slab_allocator<ArenaAllocator> ALLOCATOR {ArenaAllocator {}};
}

void* malloc(size_t size) {
	return ALLOCATOR.alloc(size);
}

void* realloc(void* old, size_t new_size) {
	if (!new_size) {
		ALLOCATOR.free(old);
		return nullptr;
	}

	void* ptr = ALLOCATOR.alloc(new_size);
	if (!ptr) {
		return nullptr;
	}
	if (old) {
		auto old_size = ALLOCATOR.get_size_for_allocation(old);
		memcpy(ptr, old, new_size < old_size ? new_size : old_size);
		ALLOCATOR.free(old);
	}
	return ptr;
}

void free(void* ptr) {
	ALLOCATOR.free(ptr);
}

__attribute__((noreturn)) void exit(int status) {
	sys_process_exit(status);
}

__attribute__((noreturn)) void abort() {
	__builtin_trap();
}
