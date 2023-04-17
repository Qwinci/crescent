#include "event_queue.h"
#include "arch/misc.h"
#include "assert.h"
#include "mem/allocator.h"
#include "sched.h"

#define EVENT_QUEUE_MAX_SIZE 255

bool event_queue_get(EventQueue* self, Event* event) {
	void* flags = enter_critical();
	if (!self->events_len) {
		return false;
	}
	*event = self->events[self->event_consumer_ptr];
	--self->events_len;
	self->event_consumer_ptr = (self->event_consumer_ptr + 1) % EVENT_QUEUE_MAX_SIZE;
	leave_critical(flags);
	return true;
}

void event_queue_push(EventQueue* self, Event event) {
	void* flags = enter_critical();
	if (self->notify_target) {
		sched_unblock(self->notify_target);
	}
	if (!self->events) {
		self->events = kmalloc(EVENT_QUEUE_MAX_SIZE * sizeof(Event));
		assert(self->events);
	}
	if (self->events_len == EVENT_QUEUE_MAX_SIZE) {
		leave_critical(flags);
		kprintf("[kernel][input]: warning: event queue full, discarding new event\n");
		return;
	}
	self->events[self->event_producer_ptr] = event;
	++self->events_len;
	self->event_producer_ptr = (self->event_producer_ptr + 1) % EVENT_QUEUE_MAX_SIZE;
	leave_critical(flags);
}