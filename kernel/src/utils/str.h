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
	return lhs.len == rhs.len && strncmp(lhs.data, rhs.data, lhs.len) == 0;
}

static inline bool str_starts_with(Str lhs, Str rhs) {
	if (lhs.len < rhs.len) {
		return false;
	}
	return strncmp(lhs.data, rhs.data, rhs.len) == 0;
}

static inline bool str_strip_prefix(Str* lhs, Str rhs) {
	if (!str_starts_with(*lhs, rhs)) {
		return false;
	}
	lhs->data += rhs.len;
	lhs->len -= rhs.len;
	return true;
}
