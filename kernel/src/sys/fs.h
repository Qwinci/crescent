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

void sys_open(void* state);
void sys_open_ex(void* state);
void sys_read(void* state);
void sys_seek(void* state);
void sys_write(void* state);
void sys_stat(void* state);
void sys_opendir(void* state);
void sys_closedir(void* state);
void sys_readdir(void* state);

int kernel_fs_open_ex(const char* dev_name, usize dev_len, const char* path, struct VNode** ret);
int kernel_fs_open(const char* path, struct VNode** ret);
