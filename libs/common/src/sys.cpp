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

int sys_service_create(const CrescentStringView* features, size_t feature_count) {
	return static_cast<int>(syscall(SYS_SERVICE_CREATE, features, feature_count));
}

int sys_service_get(CrescentHandle& handle, const CrescentStringView* needed_features, size_t feature_count) {
	return static_cast<int>(syscall(SYS_SERVICE_GET, &handle, needed_features, feature_count));
}

int sys_socket_create(CrescentHandle& handle, SocketType type, int flags) {
	return static_cast<int>(syscall(SYS_SOCKET_CREATE, &handle, type, flags));
}

int sys_socket_connect(CrescentHandle handle, SocketAddress& address) {
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

int sys_socket_receive(CrescentHandle handle, void* data, size_t size, size_t& actual) {
	return static_cast<int>(syscall(SYS_SOCKET_RECEIVE, handle, data, size, &actual));
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
