#pragma once
#include "types.hpp"

struct ExceptionFrame {
	u64 x[31];
	u64 sp;
	u64 esr_el1;
	u64 far_el1;
	u64 elr_el1;
	u64 spsr_el1;
};
