#include "arch/map.h"

static const usize X86_HUGEPAGE_SIZE = 0x200000;

usize arch_get_hugepage_size() {
	return X86_HUGEPAGE_SIZE;
}

void* arch_create_map() {
	return NULL;
}

void arch_map_page(void* map, usize virt, usize phys, PageFlags flags) {

}

void arch_unmap_page(void* map, usize virt) {

}