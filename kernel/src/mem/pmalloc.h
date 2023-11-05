#pragma once
#include "page.h"
#include "types.h"

void pmalloc_add_mem(void* base, usize size);
Page* pmalloc(usize count);
Page* pcalloc(usize count);
void pfree(Page* ptr, usize count);