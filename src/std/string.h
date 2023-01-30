#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* memset(void* dest, int ch, size_t size);
void* memcpy(void* dest, const void* src, size_t size);
int memcmp(const void* lhs, const void* rhs, size_t size);
size_t strlen(const char* str);
char* strcpy(char* dest, const char* src);
char* strncpy(char* dest, const char* src, size_t size);
char* strcat(char* dest, const char* src);
int strcmp(const char* lhs, const char* rhs);
int strncmp(const char* lhs, const char* rhs, size_t size);
int toupper(int ch);
int tolower(int ch);
int isprint(int ch);
int isxdigit(int ch);
int isdigit(int ch);
int isspace(int ch);

#ifdef __cplusplus
}
#endif