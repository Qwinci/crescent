#include "page.hpp"

extern PRegion* PREGIONS_START;
extern PRegion* PREGIONS_END;

Page* Page::from_phys(usize phys) {
	for (PRegion* region = PREGIONS_START; region; region = region->next) {
		if (region->contains(phys)) {
			return &region->pages[(phys - region->phys_base) / PAGE_SIZE];
		}
	}
	return nullptr;
}
