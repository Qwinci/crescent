#include "mod.hpp"
#include "loader/limine.h"

static volatile limine_module_request MOD_REQUEST {
	.id = LIMINE_MODULE_REQUEST,
	.revision = 0,
	.response = nullptr,
	.internal_module_count = 0,
	.internal_modules = nullptr
};

bool x86_get_module(Module& module, kstd::string_view name) {
	if (!MOD_REQUEST.response) {
		return false;
	}

	for (usize i = 0; i < MOD_REQUEST.response->module_count; ++i) {
		auto mod = MOD_REQUEST.response->modules[i];
		if (mod->cmdline == name) {
			module = {
				.data = mod->address,
				.size = mod->size
			};
			return true;
		}
	}
	return false;
}
