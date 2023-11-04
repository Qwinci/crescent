#pragma once
#include "dev.h"
#include "crescent/fs.h"

typedef struct Vfs Vfs;

typedef struct {
	GenericDevice generic;
	Vfs* vfs;
} PartitionDev;

typedef struct {
	struct VNode* node;
	usize cursor;
} FileData;

int sys_open(__user const char* path, size_t path_len, __user Handle* ret);
int sys_read(Handle handle, __user void* buffer, size_t size);
int sys_stat(Handle handle, __user Stat* stat);
int sys_opendir(__user const char* path, size_t path_len, __user Dir** ret);
int sys_closedir(__user Dir* dir);
int sys_readdir(__user Dir* dir, __user DirEntry* entry);

int kernel_fs_open(const char* path, struct VNode** ret);
