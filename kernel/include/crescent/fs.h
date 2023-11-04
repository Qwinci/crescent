#ifndef CRESCENT_FS_H
#define CRESCENT_FS_H
#include <stdint.h>
#include <stddef.h>
#include "handle.h"

typedef enum {
	FS_ENTRY_TYPE_FILE,
	FS_ENTRY_TYPE_DIR
} FsEntryType;

typedef struct {
	uint16_t name_len;
	char name[256];
	FsEntryType type;
} DirEntry;

typedef struct Dir Dir;

typedef struct {
	size_t size;
} Stat;

#endif