#include "crescent/sys.h"
#include "crescent/input.h"
#include <stddef.h>

size_t syscall0(size_t num) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num) : "rsi", "rdx", "r10", "r8", "r9", "r11", "rcx");
	return ret;
}

size_t syscall1(size_t num, size_t a0) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0) : "rsi", "rdx", "r10", "r8", "r9", "r11", "rcx");
	return ret;
}

size_t syscall2(size_t num, size_t a0, size_t a1) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1) : "rdx", "r10", "r8", "r9", "r11", "rcx");
	return ret;
}

size_t syscall3(size_t num, size_t a0, size_t a1, size_t a2) {
	size_t ret;
	__asm__ volatile("syscall" : "=a"(ret) : "D"(num), "a"(a0), "S"(a1), "d"(a2) : "r10", "r8", "r9", "r11", "rcx");
	return ret;
}

[[noreturn]] void sys_exit(int status) {
	syscall1(SYS_EXIT, status);
	__builtin_unreachable();
}

HandleId sys_create_thread(void (*fn)(void*), void* arg, bool detach) {
	return (HandleId) syscall3(SYS_CREATE_THREAD, (size_t) fn, (size_t) arg, (size_t) detach);
}

void sys_dprint(const char* msg, size_t len) {
	syscall2(SYS_DPRINT, (size_t) msg, len);
}

void sys_sleep(size_t ms) {
	syscall1(SYS_SLEEP, ms);
}

int sys_wait_thread(HandleId thread) {
	return (int) syscall1(SYS_WAIT_THREAD, thread);
}

void sys_wait_for_event(Event* event) {
	syscall1(SYS_WAIT_FOR_EVENT, (size_t) event);
}

bool sys_poll_event(Event* event) {
	return syscall1(SYS_POLL_EVENT, (size_t) event);
}

bool sys_shutdown(ShutdownType type) {
	return syscall1(SYS_SHUTDOWN, type);
}

static const char* LAYOUT[SCAN_MAX] = {
	[SCAN_A] = "a",
	[SCAN_B] = "b",
	[SCAN_C] = "c",
	[SCAN_0] = "0"
};

size_t strlen(const char* str) {
	size_t len = 0;
	while (*str++) ++len;
	return len;
}

_Noreturn void _start() {
	// num arg0 arg1 arg2 arg3 arg4 arg5
	// rdi rax  rsi  rdx  r10  r8   r9

	while (true) {
		Event event;
		sys_wait_for_event(&event);

		if (event.type == EVENT_KEY && event.key.pressed) {
			if (event.key.key == SCAN_F5) {
				sys_shutdown(SHUTDOWN_TYPE_REBOOT);
			}

			const char* text = LAYOUT[event.key.key];
			if (text) {
				sys_dprint(text, strlen(text));
			}
		}
	}
}
