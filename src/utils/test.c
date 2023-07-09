#include "test.h"
#include "stdio.h"

extern Test TEST_START[];
extern Test TEST_END[];

void run_tests() {
	kprintf("running tests...\n");
	kprintf("----------\n");
	usize success = 0;
	usize failed = 0;
	for (Test* test = TEST_START; test != TEST_END; ++test) {
		kprintf("running test %s... ", test->name);
		bool res = (*test->fn)();
		if (res) {
			kprintf("success\n");
			success += 1;
		}
		else {
			kprintf("failed\n");
			failed += 1;
		}
	}

	kprintf("pass: %u of %u\n", success, success + failed);
	kprintf("failed: %u of %u\n", failed, success + failed);
	kprintf("----------\n");
}
