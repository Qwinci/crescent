#pragma once
#include "utils/str.h"

typedef struct LogSink {
	int (*write)(struct LogSink* self, Str str);
	struct LogSink* prev;
	struct LogSink* next;
} LogSink;

void log_print(Str str);
void log_register_sink(LogSink* sink, bool resume);
void log_unregister_sink(LogSink* sink);
