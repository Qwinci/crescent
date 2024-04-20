#include <stdio.h>
#include "sys.hpp"

int main() {
	CrescentHandle desktop_handle;
	CrescentStringView features[] {
		{.str = "window_manager", .len = sizeof("window_manager") - 1}
	};
	auto ret = sys_service_get(desktop_handle, features, sizeof(features) / sizeof(*features));
	if (ret != 0) {
		puts("failed to get window manager service");
		return 1;
	}
	puts("[console]: found window manager service");
	CrescentHandle connection;
	ret = sys_socket_create(connection, SOCKET_TYPE_IPC);
	if (ret != 0) {
		puts("failed to create ipc socket");
		return 1;
	}
	IpcSocketAddress addr {
		.generic {.type = SOCKET_ADDRESS_TYPE_IPC},
		.target = desktop_handle
	};
	puts("[console]: connecting to window manager");
	ret = sys_socket_connect(connection, addr.generic);
	if (ret != 0) {
		puts("failed to connect to window manager");
	}
	puts("[console]: connected to window manager");
	char buf[128] {};
	size_t received;
	ret = sys_socket_receive(connection, buf, 128, received);
	if (ret != 0) {
		puts("failed to receive data");
	}
	puts("[console]: received data from window manager:");
	puts(buf);
	return 0;
}
