#include "stdlib.h"
#include "stdio.h"

extern "C" void __cxa_pure_virtual() {

}

void* operator new(size_t size) {
	auto* ptr = malloc(size);
	if (!ptr) {
		__builtin_trap();
	}
	return ptr;
}

void operator delete(void* ptr) {
	free(ptr);
}

void operator delete(void* ptr, size_t) {
	free(ptr);
}
