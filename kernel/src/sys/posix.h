#pragma once
#include "arch/misc.h"
#include "fs/vfs.h"
#include "types.h"

typedef struct {
	VNode* vnode;
	size_t offset;
	bool free;
} FdEntry;

typedef struct {
	FdEntry* fds;
	usize cap;
	usize len;
} FdTable;

int fd_table_insert(FdTable* self, VNode* vnode);

int sys_posix_open(__user const char* path, size_t path_len);
int sys_posix_close(int fd);
int sys_posix_read(int fd, __user void* buffer, size_t size);
int sys_posix_write(int fd, __user const void* buffer, size_t size);
CrescentSeekOff sys_posix_seek(int fd, CrescentSeekType type, CrescentSeekOff offset);
int sys_posix_mmap(void* hint, size_t size, int prot, void* __user* window);
