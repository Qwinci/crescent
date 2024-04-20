#include "button.hpp"
#include "context.hpp"
#include "desktop.hpp"
#include "sys.hpp"
#include <stdio.h>

static constexpr size_t US_IN_MS = 1000;
static constexpr size_t US_IN_S = US_IN_MS * 1000;

[[noreturn]] void dumb_loop() {
	while (true) {
		InputEvent event;
		sys_poll_event(event, SIZE_MAX);
		if (event.type == EVENT_TYPE_KEY) {
			if (event.key.code == SCANCODE_F1) {
				sys_shutdown(SHUTDOWN_TYPE_POWER_OFF);
			}
			else if (event.key.code == SCANCODE_R) {
				sys_shutdown(SHUTDOWN_TYPE_REBOOT);
			}
		}
	}
}

int main() {
	CrescentStringView features[] {
		{.str = "window_manager", .len = sizeof("window_manager") - 1}
	};
	auto res = sys_service_create(features, sizeof(features) / sizeof(*features));
	if (res != 0) {
		puts("failed to create desktop service");
		return 1;
	}

	CrescentHandle console_handle;
	res = sys_process_create(console_handle, "/bin/console", sizeof("/bin/console") - 1, nullptr, 0);
	if (res != 0) {
		puts("failed to create console process");
		return 1;
	}
	CrescentHandle ipc_listen_socket;
	res = sys_socket_create(ipc_listen_socket, SOCKET_TYPE_IPC);
	if (res != 0) {
		puts("failed to create ipc socket");
		return 1;
	}
	res = sys_socket_listen(ipc_listen_socket, 0);
	if (res != 0) {
		puts("failed to listen for connections");
		return 1;
	}
	puts("[desktop]: got a connection");
	CrescentHandle connection_socket;
	res = sys_socket_accept(ipc_listen_socket, connection_socket);
	if (res != 0) {
		puts("failed to accept connection");
		return 1;
	}
	puts("[desktop]: connection accepted");
	sys_close_handle(ipc_listen_socket);
	res = sys_socket_send(connection_socket, "hello from desktop to console", sizeof("hello from desktop to console"));
	if (res != 0) {
		puts("failed to send data");
		return 1;
	}
	puts("[desktop]: data sent");
	sys_close_handle(connection_socket);

	//dumb_loop();

	DevLinkRequest request {
		.type = DevLinkRequestType::GetDevices,
		.data {
			.get_devices {
				.type = CrescentDeviceType::Fb
			}
		}
	};
	char response[DEVLINK_BUFFER_SIZE];
	DevLink link {
		.request = &request,
		.response_buffer = response,
		.response_buf_size = DEVLINK_BUFFER_SIZE
	};
	auto status = sys_devlink(link);
	request.type = DevLinkRequestType::OpenDevice;
	request.data.open_device.device = link.response->get_devices.names[0];
	request.data.open_device.device_len = link.response->get_devices.name_sizes[0];
	status = sys_devlink(link);
	if (status != 0) {
		puts("failed to open device");
		return 1;
	}
	auto handle = link.response->open_device.handle;

	FbLink fb_link {
		.op = FbLinkOp::GetInfo
	};

	link.request = &fb_link.request;
	fb_link.request.handle = handle;
	status = sys_devlink(link);
	if (status != 0) {
		puts("devlink failed");
		return 1;
	}
	auto* fb_resp = static_cast<FbLinkResponse*>(link.response->specific);
	auto info = fb_resp->info;
	fb_link.op = FbLinkOp::Map;
	status = sys_devlink(link);
	if (status != 0) {
		puts("failed to map fb");
		return 1;
	}
	void* mapping = fb_resp->map.mapping;
	sys_close_handle(handle);

	Context ctx {
		.fb = static_cast<uint32_t*>(mapping),
		.pitch_32 = info.pitch / 4,
		.width = info.width,
		.height = info.height
	};
	Desktop desktop {ctx};

	auto window1 = std::make_unique<Window>();
	window1->set_pos(0, 0);
	window1->set_size(100, 100);
	window1->bg_color = 0x00FF00;

	auto window2 = std::make_unique<Window>();
	window2->set_pos(200, 0);
	window2->set_size(50, 50);
	window2->bg_color = 0x0000FF;

	auto window3 = std::make_unique<ButtonWindow>();
	window3->set_pos(0, TITLEBAR_HEIGHT);
	window3->set_size(50, 50);
	window3->bg_color = 0x00FFFF;
	window3->callback = [](void*) {
		sys_shutdown(SHUTDOWN_TYPE_REBOOT);
	};

	window1->add_child(std::move(window3));

	desktop.root_window->add_child(std::move(window1));
	desktop.root_window->add_child(std::move(window2));

	auto window4 = std::make_unique<ButtonWindow>();
	window4->set_pos(ctx.width - 50, 0);
	window4->set_size(50, 50);
	window4->bg_color = 0x00FFFF;
	window4->callback = [](void*) {
		sys_shutdown(SHUTDOWN_TYPE_POWER_OFF);
	};

	desktop.root_window->add_child(std::move(window4));

	desktop.handle_mouse({
		.pos {
			.x = info.width / 2,
			.y = info.height / 2
		},
		.left_pressed = false,
		.right_pressed = false,
		.middle_pressed = false
	});

	auto mouse_x = static_cast<int32_t>(info.width / 2);
	auto mouse_y = static_cast<int32_t>(info.height / 2);

	while (true) {
		InputEvent event;
		if (sys_poll_event(event, US_IN_MS * 500) == ERR_TRY_AGAIN) {

		}
		else if (event.type == EVENT_TYPE_MOUSE) {
			if (mouse_x + event.mouse.x_movement < 0) {
				mouse_x = 0;
			}
			else {
				mouse_x += event.mouse.x_movement;
				if (mouse_x > static_cast<int32_t>(info.width)) {
					mouse_x = static_cast<int32_t>(info.width);
				}
			}
			if (mouse_y + -event.mouse.y_movement < 0) {
				mouse_y = 0;
			}
			else {
				mouse_y += -event.mouse.y_movement;
				if (mouse_y > static_cast<int32_t>(info.height)) {
					mouse_y = static_cast<int32_t>(info.height);
				}
			}

			desktop.handle_mouse({
				.pos {
					.x = static_cast<uint32_t>(mouse_x),
					.y = static_cast<uint32_t>(mouse_y)
				},
				.left_pressed = event.mouse.left_pressed,
				.right_pressed = event.mouse.right_pressed,
				.middle_pressed = event.mouse.middle_pressed
			});
		}
		else if (event.type == EVENT_TYPE_KEY) {
			if (event.key.code == SCANCODE_F1) {
				sys_shutdown(SHUTDOWN_TYPE_POWER_OFF);
			}
			else if (event.key.code == SCANCODE_F5) {
				sys_shutdown(SHUTDOWN_TYPE_REBOOT);
			}
			continue;
		}

		desktop.draw();
	}
}
