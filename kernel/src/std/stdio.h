#pragma once
#include "types.h"
#include "utils/attribs.h"
#include "utils/spinlock.h"
#include <stdarg.h>

void kputs(const char* str, usize len);
void kputs_nolock(const char* str, usize len);
void kprintf(const char* fmt, ...);
void kvprintf(const char* fmt, va_list valist);
void kprintf_nolock(const char* fmt, ...);
NORETURN void panic(const char* fmt, ...);

#define PRIfg "fg"
#define PRIbg "bg"

#define COLOR_RED 0xFF0000
#define COLOR_GREEN 0x00FF00
#define COLOR_ORANGE 0xFFA500
#define COLOR_YELLOW 0xFFFF00
#define COLOR_BLUE 0x0000FF
#define COLOR_RESET COLOR_GREEN
#define COLOR_WHITE 0xFFFFFF
