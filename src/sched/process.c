#include "process.h"
#include "mem/allocator.h"
#include "string.h"
#include "mem/vm.h"

Process* ACTIVE_INPUT_PROCESS = NULL;

bool process_add_mapping(Process* process, usize base, usize size) {
	MemMapping* mapping = kmalloc(sizeof(MemMapping));
	if (!mapping) {
		return false;
	}
	mapping->next = process->mappings_end;
	mapping->base = base;
	mapping->size = size;
	if (!process->mappings) {
		process->mappings = mapping;
		process->mappings_end = mapping;
	}
	else {
		process->mappings_end->next = mapping;
		process->mappings_end = mapping;
	}
	return true;
}

void process_remove_mapping(Process* process, usize base) {
	MemMapping* prev = NULL;
	MemMapping* mapping = process->mappings;
	while (mapping) {
		if (mapping->base == base) {
			if (prev) {
				prev->next = mapping->next;
			}
			else {
				process->mappings = mapping->next;
			}
			break;
		}
		prev = mapping;
		mapping = mapping->next;
	}
}

Process* process_new_user() {
	Process* process = kmalloc(sizeof(Process));
	if (!process) {
		return NULL;
	}
	memset(process, 0, sizeof(Process));
	vm_user_init(process, 0xFF000, 0x7FFFFFFFE000 - 0xFF000);
	process->map = arch_create_user_map();
	return process;
}
