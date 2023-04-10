#pragma once
#include "arch/map.h"
#include "types.h"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);

void* vm_kernel_alloc_backed(usize count, PageFlags flags);
void vm_kernel_dealloc_backed(void* ptr, usize count);

typedef struct Task Task;

void vm_user_init(Task* task, usize base, usize size);
void vm_user_free(Task* task);
void* vm_user_alloc(Task* task, usize count);
void vm_user_dealloc(Task* task, void* ptr, usize count);

void* vm_user_alloc_backed(Task* task, usize count, PageFlags flags, void** kernel_mapping);
void vm_user_dealloc_kernel(void* kernel_mapping, usize count);
void vm_user_dealloc_backed(Task* task, void* ptr, usize count, void* kernel_mapping);