#include <stdio.h>
#include "windower/windower.hpp"

int main() {
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
			fb[y * 400 + x] = 0xFF0000;
		}
	}

	while (true) {
		asm volatile("");
	}
}
