#pragma once
#include "types.h"
#include "event_queue.h"
#include "utils/handle.h"
#include "assert.h"

typedef enum : u8 {
	TASK_STATUS_RUNNING = 0,
	TASK_STATUS_READY = 1,
	TASK_STATUS_SLEEPING = 2,
	TASK_STATUS_EXITED = 3,
	TASK_STATUS_KILLED = 4,
	TASK_STATUS_WAITING = 5,
	TASK_STATUS_MAX = 6
} TaskStatus;

typedef struct Cpu Cpu;

typedef struct Process Process;

typedef struct {
	union {
		Task* task;
		int status;
	};
	Mutex lock;
	usize refcount;
	bool exited;
} ThreadHandle;

typedef struct {
	Process* process;
	bool exited;
	Mutex lock;
} ProcessHandle;

typedef struct Task {
	char name[128];
	struct Task* next;
	struct Task* thread_prev;
	struct Task* thread_next;
	usize sleep_end;
	Cpu* cpu;
	void* map;
	Process* process;
	CrescentHandle tid;
	struct Task* signal_waiters;
	Mutex signal_waiters_lock;
	EventQueue event_queue;
	void* handler_ip;
	void* handler_sp;
	void* executor_state;
	TaskStatus status;
	u32 caps;
	u8 level;
	u8 priority;
	bool pin_level;
	bool pin_cpu;
	atomic_bool killed;
} Task;

extern Spinlock ACTIVE_INPUT_TASK_LOCK;
extern Task* ACTIVE_INPUT_TASK;
