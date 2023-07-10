#ifndef CRESCENT_SYS_H
#define CRESCENT_SYS_H
#include <stdint.h>

#define INVALID_HANDLE ((Handle) -1)

typedef uint64_t Handle;

typedef enum {
	SHUTDOWN_TYPE_REBOOT
} ShutdownType;

#define SYS_EXIT 0
#define SYS_CREATE_THREAD 1
#define SYS_DPRINT 2
#define SYS_SLEEP 3
#define SYS_WAIT_THREAD 4
#define SYS_WAIT_FOR_EVENT 5
#define SYS_POLL_EVENT 6
#define SYS_SHUTDOWN 7

#endif