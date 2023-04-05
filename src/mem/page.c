#include "page.h"

extern PRegion* pregions;
extern PRegion* pregions_end;

Page* page_from_addr(usize addr) {
	PRegion* region = pregions;
	while (region) {
		if (addr >= region->base && addr < region->base + region->size * PAGE_SIZE) {
			return &region->pages[(addr - region->base) / PAGE_SIZE];
		}
		region = region->next;
	}
	return NULL;
}