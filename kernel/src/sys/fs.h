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
int sys_open_ex(__user const char* dev, size_t dev_len, __user const char* path, size_t path_len, __user Handle* ret);
int sys_read(Handle handle, __user void* buffer, size_t size);
int sys_seek(Handle handle, CrescentSeekType seek_type, SeekOff offset);
int sys_write(Handle handle, __user const void* buffer, size_t size);
int sys_stat(Handle handle, __user CrescentStat* stat);
int sys_opendir(__user const char* path, size_t path_len, __user CrescentDir** ret);
int sys_closedir(__user CrescentDir* dir);
int sys_readdir(__user CrescentDir* dir, __user CrescentDirEntry* entry);

int kernel_fs_open_ex(const char* dev_name, usize dev_len, const char* path, struct VNode** ret);
int kernel_fs_open(const char* path, struct VNode** ret);
