#include "sys.h"

int main() {
	Handle gui_handle;
	int status = sys_create_process("posix_rootfs/gui", sizeof("posix_rootfs/gui") - 1, &gui_handle);
}
