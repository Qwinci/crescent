#ifndef CRESCENT_SYSCALLS_H
#define CRESCENT_SYSCALLS_H
#include <stddef.h>

typedef size_t CrescentHandle;

typedef struct {
	const char* str;
	size_t len;
} CrescentStringView;

#define INVALID_CRESCENT_HANDLE ((size_t) -1)

typedef enum {
	SYS_CREATE_THREAD,
	SYS_EXIT_THREAD,

	SYS_CREATE_PROCESS,
	SYS_EXIT_PROCESS,

	SYS_SLEEP,

	SYS_SYSLOG,
	SYS_DEVLINK,
	SYS_CLOSE_HANDLE,

	SYS_MAP,
	SYS_UNMAP,

	SYS_POLL_EVENT,

	SYS_SHUTDOWN
} CrescentSyscall;

typedef enum {
	ERR_MAX = -0x1000,
	ERR_INVALID_ARGUMENT,
	ERR_UNSUPPORTED,
	ERR_FAULT,
	ERR_NO_MEM,
	ERR_BUFFER_TOO_SMALL,
	ERR_TRY_AGAIN,
	ERR_NOT_EXISTS
} CrescentError;

typedef enum {
	SHUTDOWN_TYPE_POWER_OFF,
	SHUTDOWN_TYPE_REBOOT
} ShutdownType;

#define CRESCENT_PROT_READ (1U << 0)
#define CRESCENT_PROT_WRITE (1U << 1)
#define CRESCENT_PROT_EXEC (1U << 2)

#endif
