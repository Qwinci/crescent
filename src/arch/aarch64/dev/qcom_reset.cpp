#include "qcom_reset.hpp"
#include "dev/reset.hpp"

namespace regs {
	static constexpr BitField<u32, bool> ASSERT {0, 1};
}

void QcomResetController::set_assert(u32 id, bool assert) {
	assert(id < regs.size());
	auto value = space.load(regs[id]);
	if (assert) {
		value |= regs::ASSERT(true);
	}
	else {
		value &= ~regs::ASSERT;
	}
	space.store(regs[id], value);

	space.load(regs[id]);
}
