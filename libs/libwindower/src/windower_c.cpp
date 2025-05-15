#include "windower/windower.h"
#include "sys.h"
#include <cassert>
#include <cstdio>

int windower_connect(Windower* res) {
	CrescentHandle desktop_handle;
	CrescentStringView features[] {
		{.str = "window_manager", .len = sizeof("window_manager") - 1}
	};
	auto ret = sys_service_get(&desktop_handle, features, sizeof(features) / sizeof(*features));
	if (ret != 0) {
		return ret;
	}
	CrescentHandle connection;
	ret = sys_socket_create(&connection, SOCKET_TYPE_IPC, SOCK_NONE);
	if (ret != 0) {
		sys_close_handle(desktop_handle);
		return ret;
	}
	IpcSocketAddress addr {
		.generic {.type = SOCKET_ADDRESS_TYPE_IPC},
		.target = desktop_handle
	};
	ret = sys_socket_connect(connection, &addr.generic);
	if (ret != 0) {
		sys_close_handle(connection);
		sys_close_handle(desktop_handle);
		return ret;
	}

	sys_close_handle(desktop_handle);

	WindowerProtocolResponse resp {};
	size_t received;
	auto status = sys_socket_receive(connection, &resp, sizeof(resp), &received);
	assert(received == sizeof(resp));
	assert(status == 0);
	assert(resp.type == WindowerProtocolResponse::WindowerProtocolResponseConnected);

	res->control_connection = connection;
	res->event_pipe = resp.connected.event_handle;
	return 0;
}

void windower_destroy(Windower* windower) {
	if (windower->control_connection != INVALID_CRESCENT_HANDLE) {
		sys_close_handle(windower->control_connection);
		windower->control_connection = INVALID_CRESCENT_HANDLE;
	}
	if (windower->event_pipe != INVALID_CRESCENT_HANDLE) {
		sys_close_handle(windower->event_pipe);
		windower->event_pipe = INVALID_CRESCENT_HANDLE;
	}
}

int windower_create_window(Windower* windower, WindowerWindow* res, uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
	WindowerProtocolRequest req {
		.type = WindowerProtocolRequest::WindowerProtocolRequestCreateWindow,
		.create_window {
			.x = x,
			.y = y,
			.width = width,
			.height = height
		}
	};
	size_t actual;
	auto status = sys_socket_send(windower->control_connection, &req, sizeof(req), &actual);
	if (status != 0) {
		return status;
	}
	assert(actual == sizeof(req));
	WindowerProtocolResponse resp {};
	size_t received;
	status = sys_socket_receive(windower->control_connection, &resp, sizeof(resp), &received);
	if (status != 0) {
		return status;
	}
	assert(received == sizeof(resp));
	assert(resp.type == WindowerProtocolResponse::WindowerProtocolResponseWindowCreated);
	res->owner = windower;
	res->width = width;
	res->height = height;
	res->handle = resp.window_created.window_handle;
	res->fb_handle = resp.window_created.fb_handle;

	return 0;
}

void windower_window_destroy(WindowerWindow* window) {
	windower_window_close(window);
}

int windower_window_map_fb(WindowerWindow* window) {
	return sys_shared_mem_map(window->fb_handle, &window->fb_mapping);
}

WindowerProtocolWindowEvent windower_window_wait_for_event(WindowerWindow* window) {
	WindowerProtocolWindowEvent event {};
	auto status = sys_read(window->owner->event_pipe, &event, sizeof(event), nullptr);
	assert(status == 0);
	return event;
}

int windower_window_poll_event(WindowerWindow* window, WindowerProtocolWindowEvent* event) {
	CrescentStat s {};
	auto status = sys_stat(window->owner->event_pipe, &s);
	assert(status == 0);
	if (s.size >= sizeof(WindowerProtocolWindowEvent)) {
		*event = windower_window_wait_for_event(window);
		return 0;
	}
	else {
		return ERR_TRY_AGAIN;
	}
}

void windower_window_redraw(WindowerWindow* window) {
	WindowerProtocolRequest req {
		.type = WindowerProtocolRequest::WindowerProtocolRequestRedraw,
		.redraw {
			.window_handle = window->handle
		}
	};
	size_t actual;
	auto status = sys_socket_send(window->owner->control_connection, &req, sizeof(req), &actual);
	if (status != 0) {
		printf("windower: sys_socket_send failed with status %d\n", status);
	}
	assert(status == 0);
	assert(actual == sizeof(req));
	WindowerProtocolResponse resp {};
	size_t received;
	status = sys_socket_receive(window->owner->control_connection, &resp, sizeof(resp), &received);
	assert(received == sizeof(resp));
	assert(status == 0);
	assert(resp.type == WindowerProtocolResponse::WindowerProtocolResponseAck);
}

void windower_window_close(WindowerWindow* window) {
	if (window->owner) {
		WindowerProtocolRequest req {
			.type = WindowerProtocolRequest::WindowerProtocolRequestCloseWindow,
			.close_window {
				.window_handle = window->handle
			}
		};
		size_t actual;
		auto status = sys_socket_send(window->owner->control_connection, &req, sizeof(req), &actual);
		assert(status == 0);
		assert(actual == sizeof(req));
		WindowerProtocolResponse resp {};
		size_t received;
		status = sys_socket_receive(window->owner->control_connection, &resp, sizeof(resp), &received);
		assert(received == sizeof(resp));
		assert(status == 0);
		assert(resp.type == WindowerProtocolResponse::WindowerProtocolResponseAck);
		window->owner = nullptr;
	}

	if (window->fb_mapping) {
		sys_unmap(window->fb_mapping, (window->width * window->height * 4 + 0xFFF) & ~0xFFF);
		window->fb_mapping = nullptr;
	}
	if (window->fb_handle != INVALID_CRESCENT_HANDLE) {
		sys_close_handle(window->fb_handle);
		window->fb_handle = INVALID_CRESCENT_HANDLE;
	}
}
