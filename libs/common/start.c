#include "sys.h"

int main();

__attribute__((noreturn)) void _start() {
	sys_exit(main());
}
