#include "evm.hpp"

evm::Evm::~Evm() {
	for (auto& page : pages) {
		auto guard = page.lock.lock();
		if (--page.ref_count == 0) {
			pfree(page.phys(), 1);
		}
	}
}
