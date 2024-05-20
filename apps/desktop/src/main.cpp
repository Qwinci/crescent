#include "button.hpp"
#include "context.hpp"
#include "desktop.hpp"
#include "sys.hpp"
#include "windower/protocol.hpp"
#include <stdio.h>
#include <string.h>

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

template<typename T>
struct NoDestroy {
	template<typename... Args>
	constexpr explicit NoDestroy(Args&&... args) {
		new (data) T {std::forward<Args&&>(args)...};
	}

	constexpr T* operator->() {
		return std::launder(reinterpret_cast<T*>(data));
	}

	constexpr const T* operator->() const {
		return std::launder(reinterpret_cast<const T*>(data));
	}

	constexpr T& operator*() {
		return *std::launder(reinterpret_cast<T*>(data));
	}

	constexpr const T& operator*() const {
		return *std::launder(reinterpret_cast<const T*>(data));
	}

private:
	alignas(alignof(T)) char data[sizeof(T)] {};
};

static NoDestroy<std::vector<std::pair<CrescentHandle, CrescentHandle>>> CONNECTIONS {};

void listener_thread(void* arg) {
	auto* desktop = static_cast<Desktop*>(arg);

	CrescentStringView features[] {
		{.str = "window_manager", .len = sizeof("window_manager") - 1}
	};
	auto res = sys_service_create(features, sizeof(features) / sizeof(*features));
	if (res != 0) {
		puts("failed to create desktop service");
		sys_thread_exit(1);
	}

	ProcessCreateInfo console_process_info {
		.args = nullptr,
		.arg_count = 0,
		.stdin_handle = INVALID_CRESCENT_HANDLE,
		.stdout_handle = INVALID_CRESCENT_HANDLE,
		.stderr_handle = INVALID_CRESCENT_HANDLE,
		.flags = 0
	};

	CrescentHandle console_handle;
	res = sys_process_create(console_handle, "/bin/console", sizeof("/bin/console") - 1, console_process_info);
	if (res != 0) {
		puts("failed to create console process");
		sys_thread_exit(1);
	}
	CrescentHandle ipc_listen_socket;
	res = sys_socket_create(ipc_listen_socket, SOCKET_TYPE_IPC, SOCK_NONE);
	if (res != 0) {
		puts("failed to create ipc socket");
		sys_thread_exit(1);
	}

	while (true) {
		res = sys_socket_listen(ipc_listen_socket, 0);
		if (res != 0) {
			puts("failed to listen for connections");
			sys_thread_exit(1);
		}
		puts("[desktop]: got a connection");
		CrescentHandle connection_socket;
		res = sys_socket_accept(ipc_listen_socket, connection_socket, SOCK_NONBLOCK);
		if (res != 0) {
			puts("failed to accept connection");
			sys_thread_exit(1);
		}
		puts("[desktop]: connection accepted");

		IpcSocketAddress peer_addr {};
		res = sys_socket_get_peer_name(connection_socket, peer_addr.generic);
		if (res != 0) {
			puts("failed to get peer address");
			sys_thread_exit(1);
		}

		CONNECTIONS->push_back({peer_addr.target, connection_socket});
	}
}

namespace protocol = windower::protocol;

int main() {
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

	Context ctx {};
	ctx.fb = static_cast<uint32_t*>(mapping);
	ctx.pitch_32 = info.pitch / 4;
	ctx.width = info.width;
	ctx.height = info.height;
	Desktop desktop {ctx};

	CrescentHandle listener_thread_handle;
	status = sys_thread_create(listener_thread_handle, "listener", sizeof("listener") - 1, listener_thread, &desktop);
	if (status != 0) {
		puts("[desktop]: failed to create listener thread");
		return 1;
	}

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
		for (auto [proc_handle, connection] : *CONNECTIONS) {
			protocol::Request req {};
			size_t received;
			auto req_status = sys_socket_receive(connection, &req, sizeof(req), received);
			if (req_status == ERR_TRY_AGAIN) {
				continue;
			}

			// todo check status and size

			protocol::Response resp {};

			switch (req.type) {
				case protocol::Request::CreateWindow:
				{
					auto window = std::make_unique<Window>();
					window->set_pos(req.create_window.x, req.create_window.y);
					window->set_size(req.create_window.width, req.create_window.height);

					// todo check errors
					CrescentHandle fb_shared_mem_handle;
					sys_shared_mem_alloc(fb_shared_mem_handle, window->rect.width * window->rect.height * 4);
					CrescentHandle fb_shareable_handle;
					sys_shared_mem_share(fb_shared_mem_handle, proc_handle, fb_shareable_handle);
					sys_shared_mem_map(fb_shared_mem_handle, reinterpret_cast<void**>(&window->fb));
					memset(window->fb, 0, window->rect.width * window->rect.height * 4);
					sys_close_handle(fb_shared_mem_handle);

					auto* window_ptr = window.get();

					desktop.root_window->add_child(std::move(window));

					desktop.ctx.dirty_rects.push_back({
						.x = req.create_window.x,
						.y = req.create_window.y,
						.width = req.create_window.width + BORDER_WIDTH * 2,
						.height = req.create_window.height + TITLEBAR_HEIGHT + BORDER_WIDTH
					});

					resp.type = protocol::Response::WindowCreated;
					// todo make this an opaque handle instead so it can be verified
					resp.window_created.window_handle = window_ptr;
					resp.window_created.fb_handle = fb_shareable_handle;

					// todo check status
					while (sys_socket_send(connection, &resp, sizeof(resp)) == ERR_TRY_AGAIN);

					break;
				}
				case protocol::Request::CloseWindow:
				{
					auto* window = static_cast<Window*>(req.close_window.window_handle);

					for (size_t i = 0; i < window->parent->children.size(); ++i) {
						if (window->parent->children[i].get() == window) {
							auto dirty_rect = window->get_abs_rect();
							if (!window->no_decorations) {
								dirty_rect.width += BORDER_WIDTH * 2;
								dirty_rect.height += TITLEBAR_HEIGHT + BORDER_WIDTH;
							}
							ctx.dirty_rects.push_back(dirty_rect);

							window->parent->children.erase(
								window->parent->children.begin() +
								static_cast<std::vector<std::unique_ptr<Window>>::difference_type>(i));
							break;
						}
					}

					break;
				}
			}
		}

		InputEvent event;
		if (sys_poll_event(event, US_IN_MS) == ERR_TRY_AGAIN) {

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
