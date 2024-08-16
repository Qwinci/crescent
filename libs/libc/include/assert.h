#ifndef _ASSERT_H
#define _ASSERT_H

__attribute__((noreturn)) void __assert_fail(const char* __expr, const char* __file, unsigned int __line, const char* __func);

#ifndef NDEBUG
#define assert(expr) ((expr) ? (void) 0 : __assert_fail(#expr, __FILE__, __LINE__, __func__))
#else
#define assert(...)
#endif

#endif
