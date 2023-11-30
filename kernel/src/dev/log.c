#include "log.h"
#include "sched/mutex.h"

static LogSink* SINKS_HEAD = NULL;
static Mutex SINK_MUTEX = {};

#define LOG_SIZE 0x2000
static char BUF[LOG_SIZE + 1] = {};
static usize BUF_PTR = 0;

void log_register_sink(LogSink* sink, bool resume) {
	sink->prev = NULL;
	mutex_lock(&SINK_MUTEX);
	sink->next = SINKS_HEAD;
	if (SINKS_HEAD) {
		SINKS_HEAD->prev = sink;
	}
	SINKS_HEAD = sink;

	if (resume) {
		for (usize i = 0; i < BUF_PTR;) {
			usize len = strlen(BUF + BUF_PTR);
			sink->write(sink, str_new_with_len(BUF + i, len));
			i += len + 1;
		}
	}

	mutex_unlock(&SINK_MUTEX);
}

void log_unregister_sink(LogSink* sink) {
	mutex_lock(&SINK_MUTEX);
	if (sink->prev) {
		sink->prev->next = sink->next;
	}
	else {
		SINKS_HEAD = sink->next;
	}
	if (sink->next) {
		sink->next->prev = sink->prev;
	}
	mutex_unlock(&SINK_MUTEX);
	sink->prev = NULL;
	sink->next = NULL;
}

void log_print(Str str) {
	if (str.len + 1 > LOG_SIZE) {
		__builtin_trap();
	}

	mutex_lock(&SINK_MUTEX);
	if (BUF_PTR + str.len + 1 > LOG_SIZE) {
		BUF_PTR = 0;
	}

	char* start = BUF + BUF_PTR;
	memcpy(start, str.data, str.len);
	start[str.len] = 0;
	BUF_PTR += str.len + 1;

	Str slice = str_new_with_len(start, str.len);
	for (LogSink* sink = SINKS_HEAD; sink; sink = sink->next) {
		sink->write(sink, slice);
	}
	mutex_unlock(&SINK_MUTEX);
}
