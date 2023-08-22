#pragma once
#include "types.h"

typedef struct Task Task;

void* kmalloc(usize size);
void* kcalloc(usize size);
void kfree(void* ptr, usize size);