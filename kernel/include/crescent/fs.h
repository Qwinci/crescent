#ifndef CRESCENT_FS_H
#define CRESCENT_FS_H
#include <stdint.h>
#include <stddef.h>
#include "handle.h"

typedef enum {
	FS_ENTRY_TYPE_FILE,
	FS_ENTRY_TYPE_DIR
} CrescentFsEntryType;

typedef struct {
	uint16_t name_len;
	char name[256];
	CrescentFsEntryType type;
} CrescentDirEntry;

typedef struct CrescentDir CrescentDir;

typedef struct {
	size_t size;
} CrescentStat;

typedef enum {
	KSEEK_SET,
	KSEEK_END,
	KSEEK_CUR
} CrescentSeekType;

typedef intptr_t CrescentSeekOff;

#endif