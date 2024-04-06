#pragma once
#include "types.hpp"
#include "string_view.hpp"

struct Module {
	void* data;
	usize size;
};

bool x86_get_module(Module& module, kstd::string_view name);
