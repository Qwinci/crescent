#include "sys.h"
#include <iostream>

int main() {
	std::cout << "init: starting\n";

	/*
	auto type = req.data.get_devices.type;
	if (type >= CrescentDeviceType::Max) {
		*frame->ret() = ERR_INVALID_ARGUMENT;
		break;
	}
	*/

	CrescentHandle desktop_handle;
	ProcessCreateInfo desktop_info {};
	std::cout << "init: starting desktop\n";
	sys_process_create(&desktop_handle, "/bin/desktop", sizeof("/bin/desktop") - 1, &desktop_info);

	CrescentHandle qgbe_handle;
	ProcessCreateInfo qgbe_info {};
	sys_process_create(&qgbe_handle, "/bin/qgbe", sizeof("/bin/qgbe") - 1, &qgbe_info);
}
