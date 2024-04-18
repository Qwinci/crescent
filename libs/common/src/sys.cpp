#include "sys.hpp"
#include "crescent/syscall.h"

int sys_thread_create(CrescentHandle& handle, const char* name, size_t name_len, void (*fn)(void* arg), void* arg) {
	return static_cast<int>(syscall(SYS_THREAD_CREATE, &handle, name, name_len, fn, arg));
}

[[noreturn]] void sys_thread_exit(int status) {
	syscall(SYS_THREAD_EXIT, status);
	__builtin_unreachable();
}

int sys_process_create(CrescentHandle& handle, const char* path, size_t path_len, CrescentStringView* args, size_t arg_count) {
	return static_cast<int>(syscall(SYS_PROCESS_CREATE, &handle, path, path_len, args, arg_count));
}

[[noreturn]] void sys_process_exit(int status) {
	syscall(SYS_PROCESS_EXIT, status);
	__builtin_unreachable();
}

int sys_kill(CrescentHandle handle) {
	return static_cast<int>(syscall(SYS_KILL, handle));
}

int sys_get_status(CrescentHandle handle) {
	return static_cast<int>(syscall(SYS_GET_STATUS, handle));
}

int sys_sleep(uint64_t us) {
	return static_cast<int>(syscall(SYS_SLEEP, us));
}

int sys_syslog(const char* str, size_t size) {
	return static_cast<int>(syscall(SYS_SYSLOG, str, size));
}

int sys_map(void** addr, size_t size, int protection) {
	return static_cast<int>(syscall(SYS_MAP, addr, size, protection));
}

int sys_unmap(void* ptr, size_t size) {
	return static_cast<int>(syscall(SYS_UNMAP, ptr, size));
}

int sys_devlink(const DevLink& dev_link) {
	return static_cast<int>(syscall(SYS_DEVLINK, &dev_link));
}

int sys_close_handle(CrescentHandle handle) {
	return static_cast<int>(syscall(SYS_CLOSE_HANDLE, handle));
}

int sys_poll_event(InputEvent& event, size_t timeout_us) {
	return static_cast<int>(syscall(SYS_POLL_EVENT, &event, timeout_us));
}

int sys_shutdown(ShutdownType type) {
	return static_cast<int>(syscall(SYS_SHUTDOWN, type));
}
