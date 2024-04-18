#pragma once
#include "crescent/devlink.h"
#include "crescent/event.h"

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
