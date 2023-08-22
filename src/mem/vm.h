#pragma once
#include "arch/map.h"
#include "types.h"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);

void* vm_kernel_alloc_backed(usize count, PageFlags flags);
void vm_kernel_dealloc_backed(void* ptr, usize count);

typedef struct Process Process;

void vm_user_init(Process* process, usize base, usize size);
void vm_user_free(Process* process);
void* vm_user_alloc(Process* process, usize count);
void vm_user_dealloc(Process* process, void* ptr, usize count);

void* vm_user_alloc_backed(Process* process, usize count, PageFlags flags, void** kernel_mapping);
void vm_user_dealloc_kernel(void* kernel_mapping, usize count);
bool vm_user_dealloc_backed(Process* process, void* ptr, usize count, void* kernel_mapping);

bool vm_user_verify(Process* process, void* ptr, size_t len);
