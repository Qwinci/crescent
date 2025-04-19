#include "net/dns.hpp"
#include "sys.h"
#include "windower/windower.hpp"
#include <cassert>
#include <cstring>
#include <stdio.h>
#include <string>
#include <vector>

namespace protocol = windower::protocol;
using namespace std::literals;

int main() {
	CrescentHandle bash_stdout_read_handle;
	CrescentHandle bash_stdout_write_handle;
	sys_pipe_create(&bash_stdout_read_handle, &bash_stdout_write_handle, 0x1000, OPEN_NONBLOCK, 0);

	CrescentHandle bash_handle;
	ProcessCreateInfo bash_info {
		.args = nullptr,
		.arg_count = 0,
		.stdin_handle = STDIN_HANDLE,
		.stdout_handle = bash_stdout_write_handle,
		.stderr_handle = STDERR_HANDLE,
		.flags = PROCESS_STD_HANDLES
	};
	sys_process_create(&bash_handle, "/usr/bin/bash", sizeof("/usr/bin/bash") - 1, &bash_info);

	uint32_t colors[] {
		0xFF0000,
		0x00FF00,
		0x0000FF
	};
	int index = 0;

	windower::Windower windower;
	if (auto status = windower::Windower::connect(windower); status != 0) {
		puts("[console]: failed to connect to window manager");
		return 1;
	}
	windower::Window window;
	if (auto status = windower.create_window(window, 0, 0, 400, 300); status != 0) {
		puts("[console]: failed to create window");
		return 1;
	}

	if (window.map_fb() != 0) {
		puts("[console]: failed to map fb");
		return 1;
	}
	auto* fb = static_cast<uint32_t*>(window.get_fb_mapping());
	for (uint32_t y = 0; y < 32; ++y) {
		for (uint32_t x = 0; x < 32; ++x) {
			fb[y * 400 + x] = colors[index];
		}
	}

	window.redraw();

	while (true) {
		char buf[512] {};
		size_t bash_read = 0;
		sys_read(bash_stdout_read_handle, buf, sizeof(buf), &bash_read);
		if (bash_read) {
			printf("got bash stdout '%s'\n", buf);
		}

		auto event = window.wait_for_event();
		if (event.type == protocol::WindowEvent::CloseRequested) {
			window.close();
			break;
		}
		else if (event.type == protocol::WindowEvent::Key) {
			printf("[console]: received scan %u pressed %u -> %u\n", event.key.code, event.key.prev_pressed, event.key.pressed);

			if (!event.key.pressed) {
				++index;
				if (index == sizeof(colors) / sizeof(*colors)) {
					index = 0;
				}
				for (uint32_t y = 0; y < 32; ++y) {
					for (uint32_t x = 0; x < 32; ++x) {
						fb[y * 400 + x] = colors[index];
					}
				}
			}

			window.redraw();
		}
		else if (event.type == protocol::WindowEvent::Mouse) {
			if (event.mouse.left_pressed) {
				printf("[console]: mouse press\n");
			}
		}
	}

	return 0;
}
