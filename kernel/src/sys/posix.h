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

void sys_posix_open(void* state);
void sys_posix_close(void* state);
void sys_posix_read(void* state);
void sys_posix_write(void* state);
void sys_posix_seek(void* state);
void sys_posix_mmap(void* state);
void sys_posix_futex_wait(void* state);
void sys_posix_futex_wake(void* state);
