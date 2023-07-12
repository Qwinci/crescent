#ifndef CRESCENT_SYS_H
#define CRESCENT_SYS_H
#include <stdint.h>

#define INVALID_HANDLE ((Handle) -1)

typedef uint64_t Handle;

typedef enum {
	SHUTDOWN_TYPE_REBOOT
} ShutdownType;

#define SYS_CREATE_THREAD 0
#define SYS_WAIT_THREAD 1
#define SYS_EXIT 2
#define SYS_SLEEP 3
#define SYS_WAIT_FOR_EVENT 4
#define SYS_POLL_EVENT 5
#define SYS_DPRINT 6
#define SYS_SHUTDOWN 7
#define SYS_REQUEST_CAP 8
#define SYS_MMAP 9
#define SYS_MUNMAP 10
#define SYS_CLOSE 11
#define SYS_ENUMERATE_FRAMEBUFFERS 12

#define ERR_INVALID_ARG (-1)
#define ERR_NO_PERMISSIONS (-2)
#define ERR_NO_MEM (-3)

#define PROT_READ (1 << 0)
#define PROT_WRITE (1 << 1)
#define PROT_EXEC (1 << 2)

#define CAP_DIRECT_FB_ACCESS (1 << 0)
#define CAP_MAX (CAP_DIRECT_FB_ACCESS + 1)

#endif