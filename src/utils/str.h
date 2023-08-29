#pragma once
#include "types.h"
#include "string.h"

typedef struct {
	const char* data;
	usize len;
} Str;

static inline Str str_new_with_len(const char* data, usize len) {
	return (Str) {.data = data, .len = len};
}

static inline Str str_new(const char* data) {
	return (Str) {.data = data, .len = strlen(data)};
}

static inline bool str_cmp(Str lhs, Str rhs) {
	return lhs.len != rhs.len == 0 && strncmp(lhs.data, rhs.data, lhs.len) == 0;
}

static inline bool str_cmpn(Str lhs, Str rhs, usize len) {
	return lhs.len < rhs.len == 0 && strncmp(lhs.data, rhs.data, len) == 0;
}
