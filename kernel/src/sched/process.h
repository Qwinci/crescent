#pragma once
#include "mem/vmem.h"
#include "sys/posix.h"
#include "utils/handle.h"
#include "utils/rb_tree.h"
#include "signal.h"

typedef enum {
	MAPPING_FLAG_R = 1 << 0,
	MAPPING_FLAG_W = 1 << 1,
	MAPPING_FLAG_X = 1 << 2,
	MAPPING_FLAG_COW = 1 << 3,
	MAPPING_FLAG_ON_DEMAND = 1 << 4,
	MAPPING_FLAG_ZEROED = 1 << 5
} MappingFlags;

typedef struct {
	RbTreeNode hook;
	usize base;
	usize size;
	MappingFlags flags;
} Mapping;

typedef struct {
	RbTree hook;
} MappingTree;

typedef struct Process {
	HandleTable handle_table;
	FdTable fd_table;
	SignalTable signals;
	VMem low_vmem;
	VMem high_vmem;
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
bool process_add_mapping(Process* self, usize base, usize size, MappingFlags flags);
bool process_remove_mapping(Process* self, usize base);
bool process_is_mapped(Process* self, const void* start, usize size, bool rw);
Mapping* process_get_mapping_for_range(Process* self, const void* start, usize size);
void process_add_thread(Process* self, Task* task);
void process_remove_thread(Process* self, Task* task);
bool process_handle_fault(Process* process, usize addr);
void process_destroy(Process* process);
