#pragma once
#include "utils/handle.h"
#include "mem/vmem.h"
#include "utils/rb_tree.h"

typedef struct {
	RbTreeNode hook;
	usize base;
	usize size;
	bool rw;
} Mapping;

typedef struct {
	RbTree hook;
} MappingTree;

typedef struct Process {
	HandleTable handle_table;
	VMem vmem;
	Mutex vmem_lock;
	void* map;
	Mutex mapping_lock;
	MappingTree mappings;
	usize thread_count;
	Mutex threads_lock;
	Task* threads;
	Spinlock used_cpus_lock;
	u32 used_cpus[(CONFIG_MAX_CPUS + 31) / 32];
	atomic_bool killed;
} Process;

Process* process_new();
Process* process_new_user();
bool process_add_mapping(Process* self, usize base, usize size, bool rw);
bool process_remove_mapping(Process* self, usize base);
bool process_is_mapped(Process* self, const void* start, usize size, bool rw);
void process_add_thread(Process* self, Task* task);
void process_remove_thread(Process* self, Task* task);
