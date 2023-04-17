#include "tty.h"
#include "assert.h"
#include "sched/sched.h"
#include "sched/task.h"

static Task* tty_task = NULL;
extern const char* layout_fi[SCAN_MAX];

NORETURN static void tty() {
	while (true) {
		Event event;
		if (!event_queue_get(&tty_task->event_queue, &event)) {
			sched_block(TASK_STATUS_WAITING);
			event_queue_get(&tty_task->event_queue, &event);
		}

		if (event.type == EVENT_KEY_PRESS) {
			const char* text = layout_fi[event.key];
			if (text) {
				kprintf("%s", text);
			}
			else {
				kprintf("%x\n", event.key);
			}
		}
	}
}

void tty_init() {
	tty_task = arch_create_kernel_task("tty", tty, NULL);
	assert(tty_task);
	tty_task->status = TASK_STATUS_WAITING;
	tty_task->event_queue.notify_target = tty_task;
	ACTIVE_INPUT_TASK = tty_task;
}