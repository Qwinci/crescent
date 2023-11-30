#include "sys.h"

size_t syscall0(size_t num) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num) : "r11", "rcx");
	return ret;
}

size_t syscall1(size_t num, size_t a0) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0) : "r11", "rcx");
	return ret;
}

size_t syscall2(size_t num, size_t a0, size_t a1) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1) : "r11", "rcx");
	return ret;
}

size_t syscall3(size_t num, size_t a0, size_t a1, size_t a2) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2) : "r11", "rcx");
	return ret;
}

size_t syscall4(size_t num, size_t a0, size_t a1, size_t a2, size_t a3) {
	register size_t r10 __asm__("r10") = a3;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2), "r"(r10) : "r11", "rcx");
	return ret;
}

size_t syscall5(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4) {
	register size_t r10 __asm__("r10") = a3;
	register size_t r8 __asm__("r8") = a4;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8) : "r11", "rcx");
	return ret;
}

size_t syscall6(size_t num, size_t a0, size_t a1, size_t a2, size_t a3, size_t a4, size_t a5) {
	register size_t r10 __asm__("r10") = a3;
	register size_t r8 __asm__("r8") = a4;
	register size_t r9 __asm__("r9") = a5;
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2), "r"(r10), "r"(r8), "r"(r9) : "r11", "rcx");
	return ret;
}

__attribute__((noreturn)) void sys_exit(int status) {
	syscall1(SYS_EXIT, status);
	__builtin_unreachable();
}

int sys_dprint(const char* msg, size_t len) {
	return (int) syscall2(SYS_DPRINT, (size_t) msg, len);
}

int sys_create_process(const char* path, size_t path_len, Handle* ret) {
	return (int) syscall3(SYS_CREATE_PROCESS, (size_t) path, (size_t) path_len, (size_t) ret);
}

int sys_kill_process(Handle handle) {
	return (int) syscall1(SYS_KILL_PROCESS, handle);
}

int sys_wait_process(Handle handle) {
	return (int) syscall1(SYS_WAIT_PROCESS, handle);
}

Handle sys_create_thread(void (*fn)(void*), void* arg) {
	return (Handle) syscall2(SYS_CREATE_THREAD, (size_t) fn, (size_t) arg);
}

int sys_kill_thread(Handle thread) {
	return (int) syscall1(SYS_KILL_THREAD, (size_t) thread);
}

void sys_sleep(size_t ms) {
	syscall1(SYS_SLEEP, ms);
}

int sys_wait_thread(Handle thread) {
	return (int) syscall1(SYS_WAIT_THREAD, thread);
}

int sys_wait_for_event(Event* event) {
	return (int) syscall1(SYS_WAIT_FOR_EVENT, (size_t) event);
}

int sys_poll_event(Event* event) {
	return (int) syscall1(SYS_POLL_EVENT, (size_t) event);
}

int sys_shutdown(ShutdownType type) {
	return (int) syscall1(SYS_SHUTDOWN, type);
}

bool sys_request_cap(uint32_t cap) {
	return syscall1(SYS_REQUEST_CAP, cap);
}

void* sys_mmap(size_t size, int protection) {
	return (void*) syscall2(SYS_MMAP, size, protection);
}

int sys_munmap(void* ptr, size_t size) {
	return (int) syscall2(SYS_MUNMAP, (size_t) ptr, size);
}

int sys_close(Handle handle) {
	return (int) syscall1(SYS_CLOSE, handle);
}

int sys_devmsg(Handle handle, size_t msg, void* data) {
	return (int) syscall3(SYS_DEVMSG, (size_t) handle, msg, (size_t) data);
}

int sys_devenum(DeviceType type, DeviceInfo* res, size_t* count) {
	return (int) syscall3(SYS_DEVENUM, (size_t) type, (size_t) res, (size_t) count);
}

int sys_open(const char* path, size_t path_len, Handle* ret) {
	return (int) syscall3(SYS_OPEN, (size_t) path, path_len, (size_t) ret);
}

int sys_open_ex(const char* dev, size_t dev_len, const char* path, size_t path_len, Handle* ret) {
	return (int) syscall5(SYS_OPEN_EX, (size_t) dev, dev_len, (size_t) path, path_len, (size_t) ret);
}

int sys_read(Handle handle, void* buffer, size_t size) {
	return (int) syscall3(SYS_READ, handle, (size_t) buffer, size);
}

int sys_write(Handle handle, const void* buffer, size_t size) {
	return (int) syscall3(SYS_WRITE, handle, (size_t) buffer, size);
}

int sys_seek(Handle handle, CrescentSeekType type, SeekOff offset) {
	return (int) syscall3(SYS_WRITE, handle, (size_t) type, (size_t) offset);
}

int sys_stat(Handle handle, CrescentStat* stat) {
	return (int) syscall2(SYS_STAT, handle, (size_t) stat);
}

int sys_opendir(const char* path, size_t path_len, CrescentDir** ret) {
	return (int) syscall3(SYS_OPENDIR, (size_t) path, path_len, (size_t) ret);
}

int sys_closedir(CrescentDir* dir) {
	return (int) syscall1(SYS_CLOSEDIR, (size_t) dir);
}

int sys_readdir(CrescentDir* dir, CrescentDirEntry* entry) {
	return (int) syscall2(SYS_READDIR, (size_t) dir, (size_t) entry);
}

int sys_write_fs_base(size_t value) {
	return (int) syscall1(SYS_WRITE_FS_BASE, value);
}

int sys_write_gs_base(size_t value) {
	return (int) syscall1(SYS_WRITE_GS_BASE, value);
}

int sys_posix_open(const char* path, size_t path_len) {
	return (int) syscall2(SYS_POSIX_OPEN, (size_t) path, path_len);
}

int sys_posix_close(int fd) {
	return (int) syscall1(SYS_POSIX_CLOSE, (size_t) fd);
}

int sys_posix_read(int fd, void* buffer, size_t size) {
	return (int) syscall3(SYS_POSIX_READ, (size_t) fd, (size_t) buffer, size);
}

int sys_posix_write(int fd, const void* buffer, size_t size) {
	return (int) syscall3(SYS_POSIX_WRITE, (size_t) fd, (size_t) buffer, size);
}

SeekOff sys_posix_seek(int fd, CrescentSeekType type, SeekOff offset) {
	return (SeekOff) syscall3(SYS_POSIX_SEEK, (size_t) fd, (size_t) type, (size_t) offset);
}

int sys_posix_mmap(void* hint, size_t size, int prot, void** window) {
	return (int) syscall4(SYS_POSIX_MMAP, (size_t) hint, size, (size_t) prot, (size_t) window);
}
