#include "crescent/syscall.h"
#include "crescent/syscalls.h"

int main();

extern "C" void _start() {
	int status = main();
	syscall(SYS_EXIT_PROCESS, status);
	__builtin_trap();
}
