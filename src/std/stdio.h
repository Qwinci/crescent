#pragma once
#include "types.h"
#include "utils/attribs.h"

void kputs(const char* str, usize len);
void kprintf(const char* fmt, ...);
NORETURN void panic(const char* fmt, ...);
