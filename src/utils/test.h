#pragma once

typedef bool (*TestFn)();

typedef struct {
	TestFn fn;
	const char* name;
} Test;

#define TEST(Name, Fn) __attribute__((section(".tests"), used)) static volatile Test __TEST_ ## Name = { \
	.fn = Fn, \
	.name = #Name \
}

void run_tests();
