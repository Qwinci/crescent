#include "assert.hpp"
#include "mem/malloc.hpp"
#include "stdio.hpp"

void* operator new(size_t size) {
	auto ret = ALLOCATOR.alloc(size);
	assert(ret);
	return ret;
}

void* operator new[](size_t size) {
	auto ret = ALLOCATOR.alloc(size);
	assert(ret);
	return ret;
}

void operator delete(void* ptr, size_t size) {
	ALLOCATOR.free(ptr, size);
}

void operator delete(void*) {
	panic("unsized operator delete called");
}

void operator delete[](void*) {
	panic("unsized array operator delete called");
}

void operator delete[](void*, size_t) {
	panic("sized array operator delete called");
}
