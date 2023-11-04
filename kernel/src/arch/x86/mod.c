#include "arch/mod.h"
#include "limine/limine.h"
#include "string.h"

static volatile struct limine_module_request MODULE_REQUEST = {
	.id = LIMINE_MODULE_REQUEST
};

Module arch_get_module(const char* name) {
	if (!MODULE_REQUEST.response) {
		return (Module) {};
	}
	for (usize i = 0; i < MODULE_REQUEST.response->module_count; ++i) {
		struct limine_file* module = MODULE_REQUEST.response->modules[i];
		if (strcmp(module->cmdline, name) == 0) {
			return (Module) {.base = module->address, .size = module->size};
		}
	}
	return (Module) {};
}
