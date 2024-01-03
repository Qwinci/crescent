#pragma once
#include "mutex.h"

#define NUM_SIGNALS 32

typedef void (*SignalHandlerFn)(int num, void* arg, void* ctx);

typedef struct {
	SignalHandlerFn fn;
	void* arg;
} SignalHandler;

typedef struct SignalQueueNode {
	struct SignalQueueNode* next;
	int num;
} SignalQueueNode;

typedef struct {
	SignalHandler handlers[NUM_SIGNALS];
	Mutex handlers_lock;

	Mutex signal_queue_lock;
	SignalQueueNode* signal_queue;
	SignalQueueNode* signal_queue_end;
} SignalTable;

typedef struct Process Process;

void sys_attach_signal(void* state);
void sys_detach_signal(void* state);
int raise_signal(Process* process, int num);
bool get_next_signal(int* num, SignalHandler* handler);
void arch_signal_leave(void* state);
