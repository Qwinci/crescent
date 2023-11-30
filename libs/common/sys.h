#pragma once
#include <stddef.h>
#include <crescent/handle.h>
#include <crescent/input.h>
#include <crescent/sys.h>
#include <crescent/dev.h>
#include <crescent/fs.h>

size_t syscall0(size_t num);
size_t syscall1(size_t num, size_t a0);
size_t syscall2(size_t num, size_t a0, size_t a1);
size_t syscall3(size_t num, size_t a0, size_t a1, size_t a2);
size_t syscall4(size_t num, size_t a0, size_t a1, size_t a2, size_t a3);
size_t syscall5(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4);
size_t syscall6(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5);
__attribute__((noreturn)) void sys_exit(int status);
int sys_dprint(const char* msg, size_t len);
int sys_create_process(const char* path, size_t path_len, Handle* ret);
int sys_kill_process(Handle handle);
int sys_wait_process(Handle handle);
Handle sys_create_thread(void (*fn)(void*), void* arg);
int sys_kill_thread(Handle thread);
void sys_sleep(size_t ms);
int sys_wait_thread(Handle thread);
int sys_wait_for_event(Event* event);
int sys_poll_event(Event* event);
int sys_shutdown(ShutdownType type);
bool sys_request_cap(uint32_t cap);
void* sys_mmap(size_t size, int protection);
int sys_munmap(void* ptr, size_t size);
int sys_close(Handle handle);
int sys_devmsg(Handle handle, size_t msg, void* data);
int sys_devenum(DeviceType type, DeviceInfo* res, size_t* count);
int sys_open(const char* path, size_t path_len, Handle* ret);
int sys_open_ex(const char* dev, size_t dev_len, const char* path, size_t path_len, Handle* ret);
int sys_read(Handle handle, void* buffer, size_t size);
int sys_write(Handle handle, const void* buffer, size_t size);
int sys_seek(Handle handle, CrescentSeekType type, SeekOff offset);
int sys_stat(Handle handle, CrescentStat* stat);
int sys_opendir(const char* path, size_t path_len, CrescentDir** ret);
int sys_closedir(CrescentDir* dir);
int sys_readdir(CrescentDir* dir, CrescentDirEntry* entry);
int sys_write_fs_base(size_t value);
int sys_write_gs_base(size_t value);

int sys_posix_open(const char* path, size_t path_len);
int sys_posix_close(int fd);
int sys_posix_read(int fd, void* buffer, size_t size);
int sys_posix_write(int fd, const void* buffer, size_t size);
SeekOff sys_posix_seek(int fd, CrescentSeekType type, SeekOff offset);
int sys_posix_mmap(void* hint, size_t size, int prot, void** window);
