#include "sys.h"

int main() {
	sys_dprint("hello\n", sizeof("hello\n") - 1);
}
