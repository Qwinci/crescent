#ifndef CRESCENT_FS_H
#define CRESCENT_FS_H
#include <stdint.h>
#include <stddef.h>
#include "handle.h"

typedef enum {
	DEVMSG_FS_OPEN,
	DEVMSG_FS_READDIR,
	DEVMSG_FS_READ,
	DEVMSG_FS_WRITE,
	DEVMSG_FS_STAT
} DevMsgFs;

typedef enum {
	FS_ENTRY_TYPE_FILE,
	FS_ENTRY_TYPE_DIR
} FsEntryType;

typedef struct {
	uint16_t name_len;
	char name[256];
	FsEntryType type;
} DirEntry;

typedef struct {
	Handle handle;
	const char* component;
	size_t component_len;
} FsOpenData;

typedef struct {
	Handle handle;
	Handle state;
	DirEntry entry;
} FsReadDirData;

typedef struct {
	Handle handle;
	void* buffer;
	size_t len;
} FsReadData;

typedef struct {
	Handle handle;
	const void* buffer;
	size_t len;
} FsWriteData;

typedef struct {
	Handle handle;
	size_t size;
} FsStatData;

#endif