#include "windower/windower.hpp"
#include "windower/protocol.hpp"
#include "sys.hpp"

namespace windower {
	Window::~Window() {
		if (owner) {
			protocol::Request req {
				.type = protocol::Request::CloseWindow,
				.close_window {
					.window_handle = handle
				}
			};
			sys_socket_send(owner->connection, &req, sizeof(req));
			// todo assert(status == 0);
			protocol::Response resp {};
			size_t received;
			sys_socket_receive(owner->connection, &resp, sizeof(resp), received);
			// todo assert(status == 0);
		}

		if (fb_mapping) {
			sys_unmap(fb_mapping, (width * height * 4 + 0xFFF) & ~0xFFF);
		}
		if (fb_handle != INVALID_CRESCENT_HANDLE) {
			sys_close_handle(fb_handle);
		}
	}

	int Window::map_fb() {
		return sys_shared_mem_map(fb_handle, &fb_mapping);
	}

	int Windower::connect(Windower& res) {
		CrescentHandle desktop_handle;
		CrescentStringView features[] {
			{.str = "window_manager", .len = sizeof("window_manager") - 1}
		};
		auto ret = sys_service_get(desktop_handle, features, sizeof(features) / sizeof(*features));
		if (ret != 0) {
			return ret;
		}
		CrescentHandle connection;
		ret = sys_socket_create(connection, SOCKET_TYPE_IPC, SOCK_NONE);
		if (ret != 0) {
			sys_close_handle(desktop_handle);
			return ret;
		}
		IpcSocketAddress addr {
			.generic {.type = SOCKET_ADDRESS_TYPE_IPC},
			.target = desktop_handle
		};
		ret = sys_socket_connect(connection, addr.generic);
		if (ret != 0) {
			sys_close_handle(connection);
			sys_close_handle(desktop_handle);
			return ret;
		}

		sys_close_handle(desktop_handle);

		res.connection = connection;
		return 0;
	}

	Windower::~Windower() {
		if (connection != INVALID_CRESCENT_HANDLE) {
			sys_close_handle(connection);
		}
	}

	int Windower::create_window(windower::Window& res, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
		protocol::Request req {
			.type = protocol::Request::CreateWindow,
			.create_window {
				.x = x,
				.y = y,
				.width = width,
				.height = height
			}
		};
		auto status = sys_socket_send(connection, &req, sizeof(req));
		if (status != 0) {
			return status;
		}
		protocol::Response resp {};
		size_t received;
		status = sys_socket_receive(connection, &resp, sizeof(resp), received);
		if (status != 0) {
			return status;
		}
		// todo verify received size and type
		res.owner = this;
		res.width = width;
		res.height = height;
		res.handle = resp.window_created.window_handle;
		res.fb_handle = resp.window_created.fb_handle;

		return 0;
	}
}

