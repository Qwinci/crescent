#pragma once
#include "types.hpp"

struct SysvInfo {
	usize ld_entry;
	usize exe_entry;
	usize ld_base;
	usize exe_phdrs_addr;
	u16 exe_phdr_count;
	u16 exe_phdr_size;
};
