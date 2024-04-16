#include "event_queue.hpp"
#include "stdio.hpp"

void EventQueue::push(InputEvent event) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();
	int next = (producer_ptr + 1) % MAX_EVENTS;
	if (next == consumer_ptr) {
		println("[kernel]: warning: discarding event");
		return;
	}
	events[producer_ptr] = event;
	producer_ptr = next;
	produce_event.signal_one();
}

bool EventQueue::consume(InputEvent& res) {
	IrqGuard irq_guard {};
	auto guard = lock.lock();

	if (consumer_ptr == producer_ptr) {
		return false;
	}

	res = events[consumer_ptr];
	consumer_ptr = (consumer_ptr + 1) % MAX_EVENTS;

	return true;
}

EventQueue GLOBAL_EVENT_QUEUE {};

