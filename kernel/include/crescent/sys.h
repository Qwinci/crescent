#ifndef CRESCENT_SYS_H
#define CRESCENT_SYS_H
#include "handle.h"

typedef enum {
	SHUTDOWN_TYPE_REBOOT
} ShutdownType;

#define SYS_CREATE_PROCESS 0
#define SYS_KILL_PROCESS 1
#define SYS_WAIT_PROCESS 2
#define SYS_CREATE_THREAD 3
#define SYS_KILL_THREAD 4
#define SYS_WAIT_THREAD 5
#define SYS_EXIT 6
#define SYS_SLEEP 7
#define SYS_WAIT_FOR_EVENT 8
#define SYS_POLL_EVENT 9
#define SYS_DPRINT 10
#define SYS_SHUTDOWN 11
#define SYS_REQUEST_CAP 12
#define SYS_MMAP 13
#define SYS_MUNMAP 14
#define SYS_CLOSE 15
#define SYS_DEVMSG 16
#define SYS_DEVENUM 17

#define SYS_OPEN 18
#define SYS_READ 19
#define SYS_STAT 20
#define SYS_OPENDIR 21
#define SYS_READDIR 22
#define SYS_CLOSEDIR 23

#define ERR_INVALID_ARG (-1)
#define ERR_NO_PERMISSIONS (-2)
#define ERR_NO_MEM (-3)
#define ERR_FAULT (-4)
#define ERR_NOT_EXISTS (-5)
#define ERR_OPERATION_NOT_SUPPORTED (-6)
#define ERR_NOT_DIR (-7)

#define KPROT_READ (1 << 0)
#define KPROT_WRITE (1 << 1)
#define KPROT_EXEC (1 << 2)

#define CAP_DIRECT_FB_ACCESS (1 << 0)
#define CAP_MANAGE_POWER (1 << 1)
#define CAP_MAX (CAP_MANAGE_POWER + 1)

#endif