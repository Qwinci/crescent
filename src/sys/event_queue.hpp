#pragma once
#include "crescent/event.h"
#include "dev/event.hpp"
#include "types.hpp"
#include "utils/spinlock.hpp"

class EventQueue {
public:
	void push(InputEvent event);
	bool consume(InputEvent& res);

	Event produce_event {};

private:
	static constexpr int MAX_EVENTS = 256;

	InputEvent events[MAX_EVENTS] {};
	Spinlock<void> lock {};
	int producer_ptr {};
	int consumer_ptr {};
};

extern EventQueue GLOBAL_EVENT_QUEUE;
