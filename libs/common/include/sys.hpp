#pragma once
#include "crescent/devlink.h"
#include "crescent/event.h"
#include "crescent/socket.h"

int sys_thread_create(CrescentHandle& handle, const char* name, size_t name_len, void (*fn)(void* arg), void* arg);
[[noreturn]] void sys_thread_exit(int status);
int sys_process_create(CrescentHandle& handle, const char* path, size_t path_len, CrescentStringView* args, size_t arg_count);
[[noreturn]] void sys_process_exit(int status);
int sys_kill(CrescentHandle handle);
int sys_get_status(CrescentHandle handle);
int sys_sleep(uint64_t us);
int sys_syslog(const char* str, size_t size);
int sys_map(void** addr, size_t size, int protection);
int sys_unmap(void* ptr, size_t size);
int sys_devlink(const DevLink& dev_link);
int sys_close_handle(CrescentHandle handle);
int sys_poll_event(InputEvent& event, size_t timeout_us);
int sys_shutdown(ShutdownType type);

int sys_service_create(const CrescentStringView* features, size_t feature_count);
int sys_service_get(CrescentHandle& handle, const CrescentStringView* needed_features, size_t feature_count);

int sys_socket_create(CrescentHandle& handle, SocketType type, int flags);
int sys_socket_connect(CrescentHandle handle, SocketAddress& address);
int sys_socket_listen(CrescentHandle handle, uint32_t port);
int sys_socket_accept(CrescentHandle handle, CrescentHandle& connection_handle, int connection_flags);
int sys_socket_send(CrescentHandle handle, const void* data, size_t size);
int sys_socket_receive(CrescentHandle handle, void* data, size_t size, size_t& actual);
int sys_socket_get_peer_name(CrescentHandle handle, SocketAddress& address);

int sys_shared_mem_alloc(CrescentHandle& handle, size_t size);
int sys_shared_mem_map(CrescentHandle handle, void** ptr);
int sys_shared_mem_share(CrescentHandle handle, CrescentHandle process_handle, CrescentHandle& result_handle);
