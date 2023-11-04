#include "cpuid.h"

Cpuid cpuid(u32 eax, u32 ecx) {
	Cpuid id = {};
	__asm__("cpuid" : "=a"(id.eax), "=b"(id.ebx), "=c"(id.ecx), "=d"(id.edx) : "a"(eax), "c"(ecx));
	return id;
}