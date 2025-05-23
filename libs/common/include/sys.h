#pragma once
#include "crescent/devlink.h"
#include "crescent/event.h"
#include "crescent/socket.h"
#include "crescent/time.h"
#include "crescent/evm.h"

#ifdef __cplusplus
#define __noreturn [[noreturn]]

extern "C" {
#else
#define __noreturn __attribute__((noreturn))
#endif

int sys_thread_create(CrescentHandle* handle, const char* name, size_t name_len, void (*fn)(void* arg), void* arg);
__noreturn void sys_thread_exit(int status);
int sys_process_create(CrescentHandle* handle, const char* path, size_t path_len, const ProcessCreateInfo* info);
__noreturn void sys_process_exit(int status);
int sys_kill(CrescentHandle handle);
int sys_get_status(CrescentHandle handle);
int sys_get_thread_id();
int sys_get_process_id();
int sys_sleep(uint64_t ns);
int sys_get_time(uint64_t* ns);
int sys_get_date_time(CrescentDateTime* time);
int sys_syslog(const char* str, size_t size);
int sys_map(void** addr, size_t size, int protection);
int sys_unmap(void* ptr, size_t size);
int sys_devlink(const DevLink* dev_link);

int sys_close_handle(CrescentHandle handle);
int sys_move_handle(CrescentHandle* handle, CrescentHandle process_handle);

int sys_poll_event(InputEvent* event, size_t timeout_ns);
int sys_shutdown(ShutdownType type);

int sys_open(CrescentHandle* handle, const char* path, size_t path_len, int flags);
int sys_read(CrescentHandle handle, void* data, size_t size, size_t* actual);
int sys_write(CrescentHandle handle, const void* data, size_t size, size_t* actual);
int sys_seek(CrescentHandle handle, int64_t offset, int whence, uint64_t* value);
int sys_stat(CrescentHandle handle, CrescentStat* stat);
int sys_list_dir(CrescentHandle handle, CrescentDirEntry* entries, size_t* count, size_t* offset);

int sys_pipe_create(
	CrescentHandle* read_handle,
	CrescentHandle* write_handle,
	size_t max_size,
	int read_flags,
	int write_flags);
int sys_replace_std_handle(CrescentHandle std_handle, CrescentHandle new_handle);

int sys_service_create(const CrescentStringView* features, size_t feature_count);
int sys_service_get(CrescentHandle* handle, const CrescentStringView* needed_features, size_t feature_count);

int sys_socket_create(CrescentHandle* handle, SocketType type, int flags);
int sys_socket_connect(CrescentHandle handle, const SocketAddress* address);
int sys_socket_listen(CrescentHandle handle, uint32_t port);
int sys_socket_accept(CrescentHandle handle, CrescentHandle* connection_handle, int connection_flags);
int sys_socket_send(CrescentHandle handle, const void* data, size_t size, size_t* actual);
int sys_socket_send_to(CrescentHandle handle, const void* data, size_t size, const SocketAddress* address);
int sys_socket_receive(CrescentHandle handle, void* data, size_t size, size_t* actual);
int sys_socket_receive_from(CrescentHandle handle, void* data, size_t size, size_t* actual, SocketAddress* address);
int sys_socket_get_peer_name(CrescentHandle handle, SocketAddress* address);

int sys_shared_mem_alloc(CrescentHandle* handle, size_t size);
int sys_shared_mem_map(CrescentHandle handle, void** ptr);
int sys_shared_mem_share(CrescentHandle handle, CrescentHandle process_handle, CrescentHandle* result_handle);

int sys_futex_wait(int* ptr, int expected, uint64_t timeout_ns);
int sys_futex_wake(int* ptr, int count);

int sys_set_fs_base(uintptr_t value);
int sys_set_gs_base(uintptr_t value);
int sys_get_fs_base(uintptr_t* value);
int sys_get_gs_base(uintptr_t* value);

int sys_get_arch_info(ArchInfo* info);

int sys_evm_create(CrescentHandle* handle);
int sys_evm_create_vcpu(CrescentHandle handle, CrescentHandle* vcpu_handle, EvmGuestState** guest_state);
int sys_evm_map(CrescentHandle handle, size_t guest, void* host, size_t size);
int sys_evm_unmap(CrescentHandle handle, size_t guest, size_t size);
int sys_evm_vcpu_run(CrescentHandle handle);
int sys_evm_vcpu_write_state(CrescentHandle handle, int changed_state);
int sys_evm_vcpu_read_state(CrescentHandle handle, int wanted_state);
int sys_evm_vcpu_trigger_irq(CrescentHandle handle, const EvmIrqInfo* info);

#undef __noreturn

#ifdef __cplusplus
}
#endif
