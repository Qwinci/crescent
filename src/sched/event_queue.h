#pragma once
#include "crescent/input.h"
#include "mutex.h"
#include "types.h"

typedef enum : u32 {
	EVENT_NONE = 0,
	EVENT_KEY_PRESS = 1 << 0,
	EVENT_KEY_RELEASE = 1 << 1,
} EventType;

typedef struct {
	EventType type;
	union {
		struct {
			Scancode key;
			Modifier mods;
		};
	};
} Event;

typedef struct {
	Event* events;
	usize events_len;
	usize event_producer_ptr;
	usize event_consumer_ptr;
	EventType subscriptions;
	struct Task* notify_target;
	Mutex lock;
} EventQueue;

bool event_queue_get(EventQueue* self, Event* event);
void event_queue_push(EventQueue* self, Event event);