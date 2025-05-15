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
