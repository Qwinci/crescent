#ifndef CRESCENT_SYSCALLS_H
#define CRESCENT_SYSCALLS_H

#include <stddef.h>
#include <stdint.h>

typedef size_t CrescentHandle;

typedef struct CrescentStringView {
	const char* str;
	size_t len;
} CrescentStringView;

#define INVALID_CRESCENT_HANDLE ((CrescentHandle) -1)

typedef enum CrescentSyscall {
	SYS_THREAD_CREATE,
	SYS_THREAD_EXIT,

	SYS_PROCESS_CREATE,
	SYS_PROCESS_EXIT,

	SYS_KILL,
	SYS_GET_STATUS,

	SYS_GET_THREAD_ID,
	SYS_GET_PROCESS_ID,

	SYS_SLEEP,
	SYS_GET_TIME,
	SYS_GET_DATE_TIME,

	SYS_SYSLOG,
	SYS_DEVLINK,

	SYS_CLOSE_HANDLE,
	SYS_MOVE_HANDLE,

	SYS_MAP,
	SYS_UNMAP,

	SYS_POLL_EVENT,

	SYS_SHUTDOWN,

	SYS_OPENAT,
	SYS_READ,
	SYS_WRITE,
	SYS_SEEK,
	SYS_STAT,
	SYS_LIST_DIR,
	SYS_PIPE_CREATE,
	SYS_REPLACE_STD_HANDLE,

	SYS_SERVICE_CREATE,
	SYS_SERVICE_GET,

	SYS_SOCKET_CREATE,
	SYS_SOCKET_CONNECT,
	SYS_SOCKET_LISTEN,
	SYS_SOCKET_ACCEPT,
	SYS_SOCKET_SEND,
	SYS_SOCKET_SEND_TO,
	SYS_SOCKET_RECEIVE,
	SYS_SOCKET_RECEIVE_FROM,
	SYS_SOCKET_GET_PEER_NAME,

	SYS_SHARED_MEM_ALLOC,
	SYS_SHARED_MEM_MAP,
	SYS_SHARED_MEM_SHARE,

	SYS_FUTEX_WAIT,
	SYS_FUTEX_WAKE,

	SYS_SET_FS_BASE,
	SYS_SET_GS_BASE,
	SYS_GET_FS_BASE,
	SYS_GET_GS_BASE,

	SYS_GET_ARCH_INFO,

	SYS_EVM_CREATE,
	SYS_EVM_CREATE_VCPU,
	SYS_EVM_MAP,
	SYS_EVM_UNMAP,
	SYS_EVM_VCPU_RUN,
	SYS_EVM_VCPU_WRITE_STATE,
	SYS_EVM_VCPU_READ_STATE,
	SYS_EVM_VCPU_TRIGGER_IRQ
} CrescentSyscall;

typedef enum CrescentError {
	ERR_MAX = -0x1000,
	ERR_INVALID_ARGUMENT,
	ERR_UNSUPPORTED,
	ERR_FAULT,
	ERR_NO_MEM,
	ERR_BUFFER_TOO_SMALL,
	ERR_TRY_AGAIN,
	ERR_TIMEOUT,
	ERR_ALREADY_EXISTS,
	ERR_NOT_EXISTS,
	ERR_NO_ROUTE_TO_HOST,
	ERR_CONNECTION_CLOSED
} CrescentError;

typedef enum ShutdownType {
	SHUTDOWN_TYPE_POWER_OFF,
	SHUTDOWN_TYPE_REBOOT,
	SHUTDOWN_TYPE_SLEEP
} ShutdownType;

#define CRESCENT_PROT_READ (1U << 0)
#define CRESCENT_PROT_WRITE (1U << 1)
#define CRESCENT_PROT_EXEC (1U << 2)

typedef struct ProcessCreateInfo {
	CrescentStringView* args;
	size_t arg_count;
	CrescentHandle stdin_handle;
	CrescentHandle stdout_handle;
	CrescentHandle stderr_handle;
	int flags;
} ProcessCreateInfo;

#define PROCESS_STD_HANDLES (1 << 0)

#define STDIN_HANDLE (CrescentHandle) (0)
#define STDOUT_HANDLE (CrescentHandle) (1)
#define STDERR_HANDLE (CrescentHandle) (2)

typedef struct CrescentStat {
	size_t size;
} CrescentStat;

typedef enum CrescentFileType {
	CRESCENT_FILE_TYPE_FILE,
	CRESCENT_FILE_TYPE_DIRECTORY
} CrescentFileType;

typedef struct CrescentDirEntry {
	char name[128];
	size_t name_len;
	CrescentFileType type;
} CrescentDirEntry;

#define OPEN_NONE 0
#define OPEN_NONBLOCK (1 << 0)

#define SEEK_START 0
#define SEEK_CURRENT 1
#define SEEK_END 2

typedef struct X86ArchInfo {
	uint64_t tsc_frequency;
} X86ArchInfo;

#ifdef __x86_64__
typedef X86ArchInfo ArchInfo;
#else
typedef void ArchInfo;
#endif

#endif
