#include "event_queue.h"
#include "assert.h"
#include "mem/allocator.h"
#include "sched.h"
#include "string.h"
#include "arch/cpu.h"

#define EVENT_QUEUE_MAX_SIZE 256

bool event_queue_get(EventQueue* self, Event* event) {
	mutex_lock(&self->lock);

	if (!self->events_len) {
		mutex_unlock(&self->lock);
		return false;
	}
	*event = self->events[self->event_consumer_ptr];
	--self->events_len;
	self->event_consumer_ptr = (self->event_consumer_ptr + 1) % EVENT_QUEUE_MAX_SIZE;
	mutex_unlock(&self->lock);
	return true;
}

void event_queue_push(EventQueue* self, Event event) {
	mutex_lock(&self->lock);

	if (self->notify_target) {
		sched_unblock(self->notify_target);
	}
	if (!self->events) {
		self->events = kmalloc(EVENT_QUEUE_MAX_SIZE * sizeof(Event));
		assert(self->events);
		memset(self->events, 0, EVENT_QUEUE_MAX_SIZE * sizeof(Event));
	}
	if (self->events_len == EVENT_QUEUE_MAX_SIZE) {
		mutex_unlock(&self->lock);
		return;
	}
	self->events[self->event_producer_ptr] = event;
	++self->events_len;
	self->event_producer_ptr = (self->event_producer_ptr + 1) % EVENT_QUEUE_MAX_SIZE;

	mutex_unlock(&self->lock);
}