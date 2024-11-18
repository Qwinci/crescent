#include "sys.hpp"
#include "crescent/syscall.h"

int sys_thread_create(CrescentHandle& handle, const char* name, size_t name_len, void (*fn)(void* arg), void* arg) {
	return static_cast<int>(syscall(SYS_THREAD_CREATE, &handle, name, name_len, fn, arg));
}

[[noreturn]] void sys_thread_exit(int status) {
	syscall(SYS_THREAD_EXIT, status);
	__builtin_unreachable();
}

int sys_process_create(CrescentHandle& handle, const char* path, size_t path_len, ProcessCreateInfo& info) {
	return static_cast<int>(syscall(SYS_PROCESS_CREATE, &handle, path, path_len, &info));
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

int sys_get_thread_id() {
	return static_cast<int>(syscall(SYS_GET_THREAD_ID));
}

int sys_sleep(uint64_t us) {
	return static_cast<int>(syscall(SYS_SLEEP, us));
}

int sys_get_time(uint64_t* us) {
	return static_cast<int>(syscall(SYS_GET_TIME, us));
}

int sys_get_date_time(CrescentDateTime& time) {
	return static_cast<int>(syscall(SYS_GET_DATE_TIME, &time));
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

int sys_move_handle(CrescentHandle& handle, CrescentHandle process_handle) {
	return static_cast<int>(syscall(SYS_MOVE_HANDLE, &handle, process_handle));
}

int sys_poll_event(InputEvent& event, size_t timeout_us) {
	return static_cast<int>(syscall(SYS_POLL_EVENT, &event, timeout_us));
}

int sys_shutdown(ShutdownType type) {
	return static_cast<int>(syscall(SYS_SHUTDOWN, type));
}

int sys_open(CrescentHandle& handle, const char* path, size_t path_len, int flags) {
	return static_cast<int>(syscall(SYS_OPENAT, &handle, INVALID_CRESCENT_HANDLE, path, path_len, flags));
}

int sys_read(CrescentHandle handle, void* data, size_t size, size_t* actual) {
	return static_cast<int>(syscall(SYS_READ, handle, data, size, actual));
}

int sys_write(CrescentHandle handle, const void* data, size_t size, size_t* actual) {
	return static_cast<int>(syscall(SYS_WRITE, handle, data, size, actual));
}

int sys_seek(CrescentHandle handle, int64_t offset, int whence, uint64_t* value) {
	return static_cast<int>(syscall(SYS_SEEK, handle, offset, whence, value));
}

int sys_stat(CrescentHandle handle, CrescentStat& stat) {
	return static_cast<int>(syscall(SYS_STAT, handle, &stat));
}

int sys_list_dir(CrescentHandle handle, CrescentDirEntry* entries, size_t* count, size_t* offset) {
	return static_cast<int>(syscall(SYS_LIST_DIR, handle, entries, count, offset));
}

int sys_pipe_create(
	CrescentHandle& read_handle,
	CrescentHandle& write_handle,
	size_t max_size,
	int read_flags,
	int write_flags) {
	return static_cast<int>(syscall(SYS_PIPE_CREATE, &read_handle, &write_handle, max_size, read_flags, write_flags));
}

int sys_replace_std_handle(CrescentHandle std_handle, CrescentHandle new_handle) {
	return static_cast<int>(syscall(SYS_REPLACE_STD_HANDLE, std_handle, new_handle));
}

int sys_service_create(const CrescentStringView* features, size_t feature_count) {
	return static_cast<int>(syscall(SYS_SERVICE_CREATE, features, feature_count));
}

int sys_service_get(CrescentHandle& handle, const CrescentStringView* needed_features, size_t feature_count) {
	return static_cast<int>(syscall(SYS_SERVICE_GET, &handle, needed_features, feature_count));
}

int sys_socket_create(CrescentHandle& handle, SocketType type, int flags) {
	return static_cast<int>(syscall(SYS_SOCKET_CREATE, &handle, type, flags));
}

int sys_socket_connect(CrescentHandle handle, const SocketAddress& address) {
	return static_cast<int>(syscall(SYS_SOCKET_CONNECT, handle, &address));
}

int sys_socket_listen(CrescentHandle handle, uint32_t port) {
	return static_cast<int>(syscall(SYS_SOCKET_LISTEN, handle, port));
}

int sys_socket_accept(CrescentHandle handle, CrescentHandle& connection_handle, int connection_flags) {
	return static_cast<int>(syscall(SYS_SOCKET_ACCEPT, handle, &connection_handle, connection_flags));
}

int sys_socket_send(CrescentHandle handle, const void* data, size_t size) {
	return static_cast<int>(syscall(SYS_SOCKET_SEND, handle, data, size));
}

int sys_socket_send_to(CrescentHandle handle, const void* data, size_t size, const SocketAddress& address) {
	return static_cast<int>(syscall(SYS_SOCKET_SEND_TO, handle, data, size, &address));
}

int sys_socket_receive(CrescentHandle handle, void* data, size_t size, size_t& actual) {
	return static_cast<int>(syscall(SYS_SOCKET_RECEIVE, handle, data, size, &actual));
}

int sys_socket_receive_from(CrescentHandle handle, void* data, size_t size, size_t& actual, SocketAddress& address) {
	return static_cast<int>(syscall(SYS_SOCKET_RECEIVE_FROM, handle, data, size, &actual, &address));
}

int sys_socket_get_peer_name(CrescentHandle handle, SocketAddress& address) {
	return static_cast<int>(syscall(SYS_SOCKET_GET_PEER_NAME, handle, &address));
}

int sys_shared_mem_alloc(CrescentHandle& handle, size_t size) {
	return static_cast<int>(syscall(SYS_SHARED_MEM_ALLOC, &handle, size));
}

int sys_shared_mem_map(CrescentHandle handle, void** ptr) {
	return static_cast<int>(syscall(SYS_SHARED_MEM_MAP, handle, ptr));
}

int sys_shared_mem_share(CrescentHandle handle, CrescentHandle process_handle, CrescentHandle& result_handle) {
	return static_cast<int>(syscall(SYS_SHARED_MEM_SHARE, handle, process_handle, &result_handle));
}

int sys_futex_wait(int* ptr, int expected, uint64_t timeout_ns) {
	return static_cast<int>(syscall(SYS_FUTEX_WAIT, ptr, expected, timeout_ns));
}

int sys_futex_wake(int* ptr, int count) {
	return static_cast<int>(syscall(SYS_FUTEX_WAKE, ptr, count));
}
