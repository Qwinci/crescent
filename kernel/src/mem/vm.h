#pragma once
#include "arch/map.h"
#include "types.h"
#include "sched/process.h"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);

void* vm_kernel_alloc_backed(usize count, PageFlags flags);
void vm_kernel_dealloc_backed(void* ptr, usize count);

typedef struct Process Process;

void vm_user_init(Process* process, usize base, usize size);
void vm_user_free(Process* process);
void* vm_user_alloc(Process* process, void* at, usize count);
void vm_user_dealloc(Process* process, void* ptr, usize count);

void* vm_user_alloc_backed(Process* process, void* at, usize count, PageFlags flags, void** kernel_mapping);
void* vm_user_alloc_on_demand(Process* process, void* at, usize count, MappingFlags flags, void** kernel_mapping);
void vm_user_dealloc_kernel(void* kernel_mapping, usize count);
bool vm_user_dealloc_backed(Process* process, void* ptr, usize count, void* kernel_mapping);
bool vm_user_dealloc_on_demand(Process* process, void* ptr, usize count, void* kernel_mapping);

void* vm_user_create_cow(Process* process, void* original_map, Mapping* original, void* at);
bool vm_user_dealloc_cow(Process* process, void* ptr, usize count);
