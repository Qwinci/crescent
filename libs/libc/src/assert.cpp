#include "assert.h"
#include "stdlib.h"
#include "stdio.h"

__attribute__((noreturn)) void __assert_fail(const char* expr, const char* file, unsigned int line, const char* func) {
	fprintf(stderr, "%s:%u: (%s): assertion '%s' failed\n", file, line, func, expr);
	abort();
}
