#pragma once
#include "types.hpp"

struct ResetController {
	virtual void set_assert(u32 id, bool assert) = 0;
};
