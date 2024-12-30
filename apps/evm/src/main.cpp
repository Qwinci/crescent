#include "sys.hpp"
#include "windower/windower.hpp"
#include <cassert>
#include <stdio.h>
#include <string.h>

namespace protocol = windower::protocol;

#include "vga.hpp"
#include "vm.hpp"

int main() {
	windower::Windower windower;
	if (auto status = windower::Windower::connect(windower); status != 0) {
		puts("[console]: failed to connect to window manager");
		return 1;
	}

	uint32_t width = 800;
	uint32_t height = 600;

	windower::Window window;
	if (auto status = windower.create_window(window, 0, 0, width, height); status != 0) {
		puts("[console]: failed to create window");
		return 1;
	}

	if (window.map_fb() != 0) {
		puts("[console]: failed to map fb");
		return 1;
	}
	auto* fb = static_cast<uint32_t*>(window.get_fb_mapping());

	Vm vm {fb, (width * height * 4 + 0xFFF) & ~0xFFF};
	printf("run vm\n");
	vm.run();

	vga_print_text_mem(fb);

	window.redraw();

	while (true) {
		auto event = window.wait_for_event();
		if (event.type == protocol::WindowEvent::CloseRequested) {
			window.close();
			break;
		}
		else if (event.type == protocol::WindowEvent::Key) {
			printf("[evm]: received scan %u pressed %u -> %u\n", event.key.code, event.key.prev_pressed, event.key.pressed);

			window.redraw();
		}
	}
}
