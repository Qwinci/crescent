#include "mem/allocator.hpp"

void* operator new(size_t size) {
	auto ptr = ALLOCATOR.alloc(size);
	if (!ptr) {
		// todo panic
		__builtin_trap();
	}
	return ptr;
}

void* operator new[](size_t size) {
	auto ptr = ALLOCATOR.alloc(size);
	if (!ptr) {
		// todo panic
		__builtin_trap();
	}
	return ptr;
}

void operator delete(void* ptr, size_t size) {
	ALLOCATOR.free(ptr, size);
}

void operator delete[](void* ptr, size_t size) {
	ALLOCATOR.free(ptr, size);
}

extern "C" void __cxa_pure_virtual() { // NOLINT(bugprone-reserved-identifier)

}