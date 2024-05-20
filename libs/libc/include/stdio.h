#ifndef _STDIO_H
#define _STDIO_H

#include "bits/utils.h"
#include <stdarg.h>

__begin

typedef struct FILE FILE;

extern FILE* stdin;
extern FILE* stdout;
extern FILE* stderr;

int puts(const char* __str);
__attribute__((format(printf, 1, 2))) int printf(const char* __restrict fmt, ...);
int vfprintf(FILE* __restrict __file, const char* __restrict __fmt, va_list __ap);

__end

#endif
