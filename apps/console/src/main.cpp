#include <stdio.h>
#include "windower/windower.hpp"
#include "net/dns.hpp"
#include "sys.hpp"
#include <cassert>

namespace protocol = windower::protocol;
using namespace std::literals;

int main() {
	/*auto res = dns::resolve4("google.com");
	assert(res);

	char http_req[] =
		"GET /index.html HTTP/1.0\r\n"
		"Host: www.google.com\r\n"
		"Connection: Closed\r\n"
		"\r\n";

	CrescentHandle google_sock;
	assert(sys_socket_create(google_sock, SOCKET_TYPE_TCP, 0) == 0);
	Ipv4SocketAddress google_addr {
		.generic {
			.type = SOCKET_ADDRESS_TYPE_IPV4
		},
		.ipv4 = res->value,
		.port = 80
	};
	assert(sys_socket_connect(google_sock, google_addr.generic) == 0);
	assert(sys_socket_send(google_sock, http_req, sizeof(http_req) - 1) == 0);

	char response[1024 * 64] {};
	size_t response_size = 0;
	size_t body_remaining = 0;
	bool body_check = false;
	while (true) {
		size_t received = 0;
		int err = sys_socket_receive(google_sock, response + response_size, 1024 * 64 - response_size, received);
		if (err != 0) {
			if (err == ERR_CONNECTION_CLOSED) {
				break;
			}

			printf("failed to receive data: %d\n", err);
			return -1;
		}
		response_size += received;

		if (body_check) {
			assert(body_remaining >= received);
			body_remaining -= received;

			if (!body_remaining) {
				break;
			}
		}
		else {
			size_t header_end = 0;

			for (size_t i = 0; i < response_size; ++i) {
				if (i + 3 < response_size) {
					if (response[i] == '\r' && response[i + 1] == '\n' && response[i + 2] == '\r' && response[i + 3] == '\n') {
						header_end = i;
						break;
					}
				}
				else {
					break;
				}
			}

			if (header_end) {
				std::string_view view {response, response_size};
				size_t offset = 0;
				while (true) {
					auto line_end = view.find("\r\n", offset);
					auto line = view.substr(offset, line_end - offset);

					if (line.empty()) {
						break;
					}
					else if (line.starts_with("Content-Length: ")) {
						auto int_part = line.substr(16);
						size_t len = 0;
						for (auto c : int_part) {
							assert(c >= '0' && c <= '9');
							len *= 10;
							len += c - '0';
						}
						body_remaining = len - (response_size - (header_end + 4));
						body_check = true;
						break;
					}

					offset = line_end + 2;
				}
			}
		}

		if (response_size == 1024 * 64) {
			puts("header too big");
			return -1;
		}
	}

	sys_close_handle(google_sock);
	sys_syslog(response, response_size);
	printf("received data: %s\n", response);*/

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
		auto event = window.wait_for_event();
		if (event.type == protocol::WindowEvent::CloseRequested) {
			window.close();
			if (auto status = windower.create_window(window, 0, 0, 400, 300); status != 0) {
				puts("[console]: failed to create window");
				return 1;
			}
		}
		else if (event.type == protocol::WindowEvent::Key) {
			printf("[console]: received scan %u pressed %u -> %u\n", event.key.code, event.key.prev_pressed, event.key.pressed);
		}
		else if (event.type == protocol::WindowEvent::Mouse) {
			if (event.mouse.left_pressed) {
				printf("[console]: mouse press\n");
			}
		}
	}
}
