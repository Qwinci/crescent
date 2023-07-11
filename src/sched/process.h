#pragma once
#include "utils/handle.h"
#include "mem/vmem.h"

typedef struct MemMapping {
	struct MemMapping* next;
	usize base;
	usize size;
} MemMapping;

typedef struct Process {
	HandleTable handle_table;
	VMem vmem;
	void* map;
	MemMapping* mappings;
	MemMapping* mappings_end;
	Mutex mapping_lock;
	usize thread_count;
	Mutex threads_lock;
	Task* threads;
	Spinlock used_cpus_lock;
	u32 used_cpus[(CONFIG_MAX_CPUS + 31) / 32];
} Process;

Process* process_new();
Process* process_new_user();
bool process_add_mapping(Process* process, usize base, usize size);
void process_remove_mapping(Process* process, usize base);
void process_add_thread(Process* process, Task* task);
void process_remove_thread(Process* process, Task* task);
