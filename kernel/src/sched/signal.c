#include "signal.h"
#include "arch/cpu.h"
#include "arch/executor.h"
#include "crescent/sys.h"
#include "mem/allocator.h"

void sys_attach_signal(void* state) {
	int num = (int) *executor_arg0(state);
	void (*handler)(int num, void* arg, void* ctx) = (void (*)(int, void*, void*)) * executor_arg1(state);
	void* arg = (void*) *executor_arg2(state);

	if (num >= NUM_SIGNALS) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}
	Process* process = arch_get_cur_task()->process;
	mutex_lock(&process->signals.handlers_lock);

	int ret = 0;
	if (process->signals.handlers[num].fn) {
		ret = ERR_EXISTS;
		goto cleanup;
	}
	SignalHandler* h = &process->signals.handlers[num];
	h->fn = handler;
	h->arg = arg;
cleanup:
	mutex_unlock(&process->signals.handlers_lock);
	*executor_ret0(state) = ret;
}

void sys_detach_signal(void* state) {
	int num = (int) *executor_arg0(state);

	if (num >= NUM_SIGNALS) {
		*executor_ret0(state) = ERR_INVALID_ARG;
		return;
	}

	Process* process = arch_get_cur_task()->process;
	mutex_lock(&process->signals.handlers_lock);

	SignalHandler* h = &process->signals.handlers[num];
	h->fn = NULL;
	h->arg = NULL;

	mutex_unlock(&process->signals.handlers_lock);
	*executor_ret0(state) = 0;
}

int raise_signal(Process* process, int num) {
	if (num >= NUM_SIGNALS) {
		return ERR_INVALID_ARG;
	}

	mutex_lock(&process->signals.signal_queue_lock);

	int ret = 0;
	SignalQueueNode* node = kmalloc(sizeof(SignalQueueNode));
	if (!node) {
		ret = ERR_NO_MEM;
		goto cleanup;
	}

	node->next = NULL;
	node->num = num;
	if (process->signals.signal_queue_end) {
		process->signals.signal_queue_end->next = node;
	}
	else {
		process->signals.signal_queue = node;
	}
	process->signals.signal_queue_end = node;

cleanup:
	mutex_unlock(&process->signals.signal_queue_lock);
	return ret;
}

bool get_next_signal(int* num, SignalHandler* handler) {
	Process* process = arch_get_cur_task()->process;

	mutex_lock(&process->signals.signal_queue_lock);
	if (!process->signals.signal_queue) {
		mutex_unlock(&process->signals.signal_queue_lock);
		return false;
	}

	SignalQueueNode* node = process->signals.signal_queue;
	process->signals.signal_queue = node->next;
	if (!node->next) {
		process->signals.signal_queue_end = NULL;
	}
	mutex_unlock(&process->signals.signal_queue_lock);

	*num = node->num;
	kfree(node, sizeof(SignalQueueNode));

	mutex_lock(&process->signals.handlers_lock);
	*handler = process->signals.handlers[*num];
	mutex_unlock(&process->signals.handlers_lock);
	return true;
}
