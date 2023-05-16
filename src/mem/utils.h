#pragma once
#include "types.h"

extern usize HHDM_OFFSET;

static inline void* to_virt(usize phys) {
	return (void*) (HHDM_OFFSET + phys);
}

static inline usize to_phys(void* virt) {
	return (usize) virt - HHDM_OFFSET;
}

usize to_phys_generic(void* virt);

#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))
#define ALIGNDOWN(value, align) ((value) & ~((align) - 1))