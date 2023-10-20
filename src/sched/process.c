#include "process.h"
#include "mem/allocator.h"
#include "string.h"
#include "mem/vm.h"
#include "task.h"
#include "mem/pmalloc.h"
#include "mem/utils.h"

Process* ACTIVE_INPUT_PROCESS = NULL;

Mapping* process_find_mapping(Process* self, usize addr) {
	Mapping* mapping = (Mapping*) self->mappings.hook.root;
	while (mapping) {
		if (addr < mapping->base) {
			mapping = (Mapping*) mapping->hook.left;
		}
		else if (addr > mapping->base) {
			mapping = (Mapping*) mapping->hook.right;
		}
		else {
			return mapping;
		}
	}
	return NULL;
}

bool process_is_mapped(Process* self, const void* start, usize size, bool rw) {
	usize end = (usize) start + size;

	const Mapping* mapping = (const Mapping*) self->mappings.hook.root;
	while (mapping) {
		usize mapping_start = mapping->base;
		usize mapping_end = mapping_start + mapping->size;

		if ((usize) start < mapping_end && mapping_start < end) {
			return !rw || mapping->flags & MAPPING_FLAG_W;
		}

		if ((usize) start < mapping->base) {
			mapping = (const Mapping*) mapping->hook.left;
		}
		else if ((usize) start > mapping->base) {
			mapping = (const Mapping*) mapping->hook.right;
		}
	}
	return false;
}

Mapping* process_get_mapping_for_range(Process* self, const void* start, usize size) {
	usize end = (usize) start + size;

	Mapping* mapping = (Mapping*) self->mappings.hook.root;
	while (mapping) {
		usize mapping_start = mapping->base;
		usize mapping_end = mapping_start + mapping->size;

		if ((usize) start < mapping_end && mapping_start < end) {
			return mapping;
		}

		if ((usize) start < mapping->base) {
			mapping = (Mapping*) mapping->hook.left;
		}
		else if ((usize) start > mapping->base) {
			mapping = (Mapping*) mapping->hook.right;
		}
		else {
			return mapping;
		}
	}
	return NULL;
}

bool process_add_mapping(Process* self, usize base, usize size, MappingFlags flags) {
	Mapping* mapping = kcalloc(sizeof(Mapping));
	if (!mapping) {
		return false;
	}
	mapping->base = base;
	mapping->size = size;
	mapping->flags = flags;

	Mapping* m = (Mapping*) self->mappings.hook.root;
	if (!m) {
		rb_tree_insert_root(&self->mappings.hook, &mapping->hook);
		return true;
	}

	while (true) {
		if (base < m->base) {
			if (!m->hook.left) {
				rb_tree_insert_left(&self->mappings.hook, &m->hook, &mapping->hook);
				break;
			}
			m = (Mapping*) m->hook.left;
		}
		else if (base > m->base) {
			if (!m->hook.right) {
				rb_tree_insert_right(&self->mappings.hook, &m->hook, &mapping->hook);
				break;
			}
			m = (Mapping*) m->hook.right;
		}
		else {
			assert(false && "duplicate entry in process mappings");
		}
	}

	return true;
}

bool process_remove_mapping(Process* self, usize base) {
	Mapping* mapping = process_find_mapping(self, base);
	if (!mapping) {
		return false;
	}
	rb_tree_remove(&self->mappings.hook, &mapping->hook);
	kfree(mapping, sizeof(Mapping));
	return true;
}

Process* process_new_user() {
	Process* process = kcalloc(sizeof(Process));
	if (!process) {
		return NULL;
	}
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

static PageFlags flags_to_pf(MappingFlags flags) {
	PageFlags ret = PF_USER;
	if (flags & MAPPING_FLAG_R) {
		ret |= PF_READ;
	}
	if (flags & MAPPING_FLAG_W) {
		ret |= PF_READ | PF_WRITE;
	}
	if (flags & MAPPING_FLAG_X) {
		ret |= PF_READ | PF_EXEC;
	}
	return ret;
}

bool process_handle_fault(Process* process, usize addr) {
	Mapping* mapping = process_get_mapping_for_range(process, (const void*) addr, 0);
	if (!mapping || !((mapping->flags & MAPPING_FLAG_COW) | (mapping->flags & MAPPING_FLAG_ON_DEMAND))) {
		return false;
	}
	if (!(mapping->flags & MAPPING_FLAG_R)) {
		return false;
	}

	assert(addr >= mapping->base);
	Page* page = pmalloc(1);
	if (!page) {
		// todo oom
		return false;
	}

	if (mapping->flags & MAPPING_FLAG_COW) {
		memcpy(to_virt(page->phys), (void*) ALIGNDOWN(addr, PAGE_SIZE), PAGE_SIZE);
		usize orig_addr = arch_virt_to_phys(process->map, ALIGNDOWN(addr, PAGE_SIZE));
		Page* orig_page = page_from_addr(orig_addr);
		assert(orig_page->refs > 0);
		orig_page->refs -= 1;
	}

	PageFlags flags = flags_to_pf(mapping->flags);
	arch_user_map_page(process, ALIGNDOWN(addr, PAGE_SIZE), page->phys, flags);
	arch_invalidate_mapping(process);

	return true;
}
