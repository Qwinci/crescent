#pragma once
#include "types.h"

extern usize HHDM_OFFSET;

static inline void* to_virt(usize phys) {
	return (void*) (HHDM_OFFSET + phys);
}

static inline usize to_phys(void* virt) {
	return (usize) virt - HHDM_OFFSET;
}