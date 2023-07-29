#include "process.h"
#include "mem/allocator.h"
#include "string.h"
#include "mem/vm.h"
#include "task.h"

Process* ACTIVE_INPUT_PROCESS = NULL;

bool process_add_mapping(Process* process, usize base, usize size) {
	MemMapping* mapping = kmalloc(sizeof(MemMapping));
	if (!mapping) {
		return false;
	}
	mapping->next = NULL;
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

bool process_remove_mapping(Process* process, usize base) {
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
			if (process->mappings_end == mapping) {
				process->mappings_end = prev;
			}
			kfree(mapping, sizeof(MemMapping));
			return true;
		}
		prev = mapping;
		mapping = mapping->next;
	}
	return false;
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

void process_add_thread(Process* process, Task* task) {
	task->thread_prev = NULL;
	task->thread_next = process->threads;
	if (task->thread_next) {
		task->thread_next->thread_prev = task;
	}
	process->threads = task;
	process->thread_count += 1;
}

void process_remove_thread(Process* process, Task* task) {
	mutex_lock(&process->threads_lock);
	if (task->thread_prev) {
		task->thread_prev->thread_next = task->thread_next;
	}
	else {
		process->threads = task->thread_next;
	}
	if (task->thread_next) {
		task->thread_next->thread_prev = task->thread_prev;
	}
	process->thread_count -= 1;
	mutex_unlock(&process->threads_lock);
}
