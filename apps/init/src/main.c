#include <stdio.h>
#include <pthread.h>
#include "sys.h"
#include <crescent/sys.h>

int sys_attach_signal(int num, void (*handler)(int num, void* arg, void* ctx), void* arg) {
	return (int) syscall3(SYS_ATTACH_SIGNAL, (size_t) num, (size_t) handler, (size_t) arg);
}

int sys_detach_signal(int num) {
	return (int) syscall1(SYS_DETACH_SIGNAL, (size_t) num);
}

int sys_raise_signal(int num) {
	return (int) syscall1(SYS_RAISE_SIGNAL, (size_t) num);
}

int sys_sigleave(void* ctx) {
	return (int) syscall1(SYS_SIGLEAVE, (size_t) ctx);
}

void* another(void*) {
	puts("in another thread");
	return NULL;
}

void signal_handler(int num, void* arg, void* ctx) {
	puts("blah");
	sys_sigleave(ctx);
}

int main() {
	puts("trying signal");
	int status = sys_attach_signal(0, signal_handler, (void*) 0xCAFE);
	printf("sys_attach_signal: %d\n", status);
	status = sys_raise_signal(0);
	printf("sys_raise_signal: %d\n", status);

	puts("creating thread");
	pthread_t thread;
	int ret = pthread_create(&thread, NULL, another, NULL);
	if (ret == 0) {
		pthread_join(thread, NULL);
	}
	else {
		puts("failed to create thread");
	}
	puts("exiting init");
	while (1);
}