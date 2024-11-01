#include "desktop.hpp"
#include "sys.hpp"
#include "windower/protocol.hpp"
#include "window.hpp"
#include <ui/button.hpp>
#include <ui/context.hpp>
#include <ui/text.hpp>
#include <cassert>
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

struct Connection {
	CrescentHandle process;
	CrescentHandle control;
	CrescentHandle event;
};

// todo mutex
static NoDestroy<std::vector<std::unique_ptr<Connection>>> CONNECTIONS {};

struct WindowInfo {
	ui::Window* window;
	Connection* connection;
	CrescentHandle event_pipe;
};

static NoDestroy<std::vector<WindowInfo>> WINDOW_TO_INFO {};

namespace protocol = windower::protocol;

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

		CrescentHandle event_read_handle;
		CrescentHandle event_write_handle;
		auto status = sys_pipe_create(
			event_read_handle,
			event_write_handle,
			64 * sizeof(protocol::WindowEvent),
			0,
			OPEN_NONBLOCK);
		assert(status == 0);
		status = sys_move_handle(event_read_handle, peer_addr.target);
		assert(status == 0);

		protocol::Response resp {};
		resp.type = protocol::Response::Connected;
		resp.connected.event_handle = event_read_handle;
		// todo check status
		while (sys_socket_send(connection_socket, &resp, sizeof(resp)) == ERR_TRY_AGAIN);

		CONNECTIONS->push_back(std::make_unique<Connection>(Connection {
			.process = peer_addr.target,
			.control = connection_socket,
			.event = event_write_handle
		}));
	}
}

void send_event_to_window(ui::Window* window, protocol::WindowEvent event) {
	CrescentHandle event_pipe = INVALID_CRESCENT_HANDLE;

	for (auto& info : *WINDOW_TO_INFO) {
		if (info.window == window) {
			event_pipe = info.event_pipe;
			break;
		}
	}

	assert(event_pipe != INVALID_CRESCENT_HANDLE);

	// todo check status
	sys_write(event_pipe, &event, 0, sizeof(event));
}

static void destroy_window(Desktop& desktop, ui::Window* window) {
	desktop.gui.destroy_window(window);

	for (size_t i = 0; i < WINDOW_TO_INFO->size(); ++i) {
		auto& iter = (*WINDOW_TO_INFO)[i];
		if (iter.window == window) {
			auto status = sys_unmap(window->fb, window->rect.width * window->rect.height * 4);
			assert(status == 0);

			WINDOW_TO_INFO->erase(
				WINDOW_TO_INFO->begin() +
				static_cast<ptrdiff_t>(i));
			break;
		}
	}
}

int main() {
	while (false) {
		DevLinkRequest request {
			.type = DevLinkRequestType::GetDevices,
			.data {
				.get_devices {
					.type = CrescentDeviceType::Sound
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
		if (status != 0) {
			puts("failed to get sound devices");
			return 1;
		}
		request.type = DevLinkRequestType::OpenDevice;
		request.data.open_device.device = link.response->get_devices.names[0];
		request.data.open_device.device_len = link.response->get_devices.name_sizes[0];
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to open sound device");
			return 1;
		}
		auto handle = link.response->open_device.handle;

		SoundLink sound_link {
			.op = SoundLinkOp::GetInfo,
			.dummy {}
		};

		link.request = &sound_link.request;
		sound_link.request.handle = handle;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to get sound info");
			return 1;
		}
		auto* sound_resp = static_cast<SoundLinkResponse*>(link.response->specific);
		printf("%d outputs\n", (int) sound_resp->info.output_count);

		sound_link.op = SoundLinkOp::GetOutputInfo;
		sound_link.get_output_info.index = 4;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to get output info");
			return 1;
		}

		auto buffer_size = sound_resp->output_info.buffer_size;

		sound_link.op = SoundLinkOp::SetActiveOutput;
		sound_link.set_active_output.id = sound_resp->output_info.id;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to set active output");
			return 1;
		}

		sound_link.op = SoundLinkOp::SetOutputParams;
		sound_link.set_output_params.params = {
			.sample_rate = 44100,
			.channels = 2,
			.fmt = SoundFormat::PcmU16
		};
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to set output params");
			return 1;
		}

		CrescentHandle file_handle;
		status = sys_open(file_handle, "/output.raw", sizeof("/output.raw") - 1, 0);
		assert(status == 0);

		CrescentStat stat {};
		status = sys_stat(file_handle, stat);
		assert(status == 0);

		std::vector<uint8_t> file_data;
		file_data.resize(stat.size);
		assert(sys_read(file_handle, file_data.data(), 0, stat.size) == 0);
		assert(sys_close_handle(file_handle) == 0);

		sound_link.op = SoundLinkOp::QueueOutput;
		sound_link.queue_output.buffer = file_data.data();
		sound_link.queue_output.len = std::min(buffer_size, stat.size);
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to queue data");
			return 1;
		}

		sound_link.op = SoundLinkOp::Play;
		sound_link.play.play = true;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to play");
			return 1;
		}
	}

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
	if (status != 0) {
		puts("failed to get devices");
		return 1;
	}
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
		puts("failed to get fb info");
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
	void* back_mapping = fb_resp->map.mapping;
	void* front_mapping = nullptr;

	bool double_buffer = info.flags & FB_LINK_DOUBLE_BUFFER;

	if (double_buffer) {
		puts("[desktop]: using double buffering");

		fb_link.op = FbLinkOp::Flip;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to flip fb");
			return 1;
		}

		fb_link.op = FbLinkOp::Map;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to map fb");
			return 1;
		}
		front_mapping = fb_resp->map.mapping;

		fb_link.op = FbLinkOp::Flip;
		status = sys_devlink(link);
		if (status != 0) {
			puts("failed to flip fb");
			return 1;
		}
	}
	else {
		puts("[desktop]: not using double buffering");
	}

	ui::Context ctx {};
	ctx.fb = static_cast<uint32_t*>(back_mapping);
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

	auto window1 = std::make_unique<DesktopWindow>(false);
	window1->set_pos(0, 0);
	window1->set_size(100, 100);
	window1->bg_color = 0x00FF00;
	window1->internal = true;

	auto window2 = std::make_unique<DesktopWindow>(false);
	window2->set_pos(200, 0);
	window2->set_size(50, 50);
	window2->bg_color = 0x0000FF;
	window2->internal = true;

	auto window3 = std::make_unique<ui::ButtonWindow>();
	window3->set_pos(0, 0);
	window3->set_size(50, 50);
	window3->bg_color = 0x00FFFF;
	window3->callback = [](void*) {
		sys_shutdown(SHUTDOWN_TYPE_REBOOT);
	};
	window3->internal = true;

	window1->add_child(std::move(window3));

	desktop.add_child(std::move(window1));
	desktop.add_child(std::move(window2));

	auto window4 = std::make_unique<ui::ButtonWindow>();
	window4->set_pos(ctx.width - 50, 0);
	window4->set_size(50, 50);
	window4->bg_color = 0x00FFFF;
	window4->callback = [](void*) {
		sys_shutdown(SHUTDOWN_TYPE_POWER_OFF);
	};
	window4->internal = true;

	desktop.add_child(std::move(window4));

	auto start_menu_text = std::make_unique<ui::TextWindow>();
	start_menu_text->set_size(desktop.taskbar->rect.height, desktop.taskbar->rect.height);
	start_menu_text->text = "Start";

	auto start_menu_button = std::make_unique<ui::ButtonWindow>();
	start_menu_button->bg_color = 0x333333;
	start_menu_button->callback = [](void* arg) {
		auto* desktop = static_cast<Desktop*>(arg);
		if (desktop->start_menu) {
			destroy_window(*desktop, desktop->start_menu);
			desktop->start_menu = nullptr;
			return;
		}

		auto menu_window = std::make_unique<DesktopWindow>(false);
		menu_window->internal = true;
		menu_window->on_close = [](ui::Window* window, void* arg) {
			auto* desktop = static_cast<Desktop*>(arg);
			destroy_window(*desktop, window);
			desktop->start_menu = nullptr;
		};
		menu_window->on_close_arg = desktop;
		menu_window->set_size(desktop->gui.ctx.width / 4, desktop->gui.ctx.height / 2);
		menu_window->set_pos(
			0,
			desktop->gui.ctx.height - menu_window->rect.height - desktop->taskbar->rect.height - BORDER_WIDTH - TITLEBAR_HEIGHT);
		menu_window->set_title("Start Menu");

		desktop->start_menu = menu_window.get();

		CrescentHandle bin_dir_handle;
		auto status = sys_open(bin_dir_handle, "/bin", sizeof("/bin") - 1, 0);
		assert(status == 0);

		size_t file_count = 0;
		size_t file_offset = 0;
		status = sys_list_dir(bin_dir_handle, nullptr, &file_count, &file_offset);
		assert(status == 0);

		std::vector<CrescentDirEntry> entries;
		entries.resize(file_count);
		status = sys_list_dir(bin_dir_handle, entries.data(), &file_count, &file_offset);
		assert(status == 0);

		uint32_t y_offset = 0;
		size_t entry_index = 0;

		constexpr uint32_t COLORS[] {
			0xFFA500,
			0xFAEB36,
			0x79C314,
			0x487DE7,
			0x4B369D,
			0x70369D
		};

		for (auto& entry : entries) {
			auto entry_button = std::make_unique<ui::ButtonWindow>();
			entry_button->set_pos(0, y_offset);
			entry_button->set_size(menu_window->rect.width, (menu_window->rect.height - TaskbarWindow::HEIGHT) / 10);
			y_offset += entry_button->rect.height;

			auto entry_text = std::make_unique<ui::TextWindow>();
			entry_text->set_size(entry_button->rect.width, entry_button->rect.height);
			entry_text->text = entry.name;
			entry_text->bg_color = COLORS[entry_index % (sizeof(COLORS) / sizeof(*COLORS))];
			entry_text->text_color = 0;

			entry_button->arg = entry_text.get();

			entry_button->add_child(std::move(entry_text));

			entry_button->callback = [](void* arg) {
				auto* text = static_cast<ui::TextWindow*>(arg);

				ProcessCreateInfo proc_info {
					.args = nullptr,
					.arg_count = 0,
					.stdin_handle = INVALID_CRESCENT_HANDLE,
					.stdout_handle = INVALID_CRESCENT_HANDLE,
					.stderr_handle = INVALID_CRESCENT_HANDLE,
					.flags = 0
				};

				CrescentHandle proc_handle;
				auto res = sys_process_create(proc_handle, text->text.data(), text->text.size(), proc_info);
				if (res != 0) {
					printf("desktop: failed to create process %s\n", text->text.c_str());
					return;
				}
				sys_close_handle(proc_handle);
			};
			++entry_index;

			menu_window->add_child(std::move(entry_button));
		}

		status = sys_close_handle(bin_dir_handle);
		assert(status == 0);

		constexpr uint32_t button_count = 3;
		constexpr uint32_t padding = 8;
		constexpr uint32_t total_padding = (button_count + 1) * padding;

		uint32_t button_width = (menu_window->rect.width - total_padding) / button_count;
		uint32_t button_height = TaskbarWindow::HEIGHT - 8;
		uint32_t button_y = menu_window->rect.height - TaskbarWindow::HEIGHT - 4;

		auto shutdown_button = std::make_unique<ui::ButtonWindow>();
		shutdown_button->set_size(button_width, button_height);
		shutdown_button->set_pos(padding, button_y);

		auto shutdown_text = std::make_unique<ui::TextWindow>();
		shutdown_text->set_size(shutdown_button->rect.width, shutdown_button->rect.height);
		shutdown_text->bg_color = 0xFFA500;
		shutdown_text->text_color = 0;
		shutdown_text->text = "Shutdown";

		shutdown_button->add_child(std::move(shutdown_text));

		auto reboot_button = std::make_unique<ui::ButtonWindow>();
		reboot_button->set_size(button_width, button_height);
		reboot_button->set_pos(shutdown_button->rect.width + padding * 2, button_y);

		auto reboot_text = std::make_unique<ui::TextWindow>();
		reboot_text->set_size(reboot_button->rect.width, reboot_button->rect.height);
		reboot_text->bg_color = 0xFFA500;
		reboot_text->text_color = 0;
		reboot_text->text = "Reboot";

		reboot_button->add_child(std::move(reboot_text));

		auto sleep_button = std::make_unique<ui::ButtonWindow>();
		sleep_button->set_size(button_width, button_height);
		sleep_button->set_pos(shutdown_button->rect.width * 2 + padding * 3, button_y);

		auto sleep_text = std::make_unique<ui::TextWindow>();
		sleep_text->set_size(sleep_button->rect.width, sleep_button->rect.height);
		sleep_text->bg_color = 0xFFA500;
		sleep_text->text_color = 0;
		sleep_text->text = "Sleep";

		sleep_button->add_child(std::move(sleep_text));

		shutdown_button->callback = [](void*) {
			sys_shutdown(SHUTDOWN_TYPE_POWER_OFF);
		};
		reboot_button->callback = [](void*) {
			sys_shutdown(SHUTDOWN_TYPE_REBOOT);
		};
		sleep_button->callback = [](void*) {
			sys_shutdown(SHUTDOWN_TYPE_SLEEP);
		};

		menu_window->add_child(std::move(shutdown_button));
		menu_window->add_child(std::move(reboot_button));
		menu_window->add_child(std::move(sleep_button));

		desktop->gui.ctx.dirty_rects.push_back({
			.x = menu_window->rect.x,
			.y = menu_window->rect.y,
			.width = menu_window->rect.width + BORDER_WIDTH * 2,
			.height = menu_window->rect.height + TITLEBAR_HEIGHT + BORDER_WIDTH
		});

		desktop->add_child(std::move(menu_window));
	};
	start_menu_button->arg = &desktop;

	start_menu_button->add_child(std::move(start_menu_text));

	desktop.taskbar->add_entry(std::move(start_menu_button));
	desktop.taskbar->add_icon(0xE81416);
	desktop.taskbar->add_icon(0xFFA500);
	desktop.taskbar->add_icon(0xFAEB36);
	desktop.taskbar->add_icon(0x79C314);
	desktop.taskbar->add_icon(0x487DE7);
	desktop.taskbar->add_icon(0x4B369D);
	desktop.taskbar->add_icon(0x70369D);

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

	desktop.taskbar->update_time(desktop.gui.ctx);

	uint64_t last_time_update_us;
	status = sys_get_time(&last_time_update_us);
	assert(status == 0);

	while (true) {
		for (size_t i = 0; i < CONNECTIONS->size();) {
			auto& connection = (*CONNECTIONS)[i];

			protocol::Request req {};
			size_t received;
			auto req_status = sys_socket_receive(connection->control, &req, sizeof(req), received);
			if (req_status == ERR_TRY_AGAIN) {
				++i;
				continue;
			}
			else if (req_status == ERR_INVALID_ARGUMENT) {
				status = sys_close_handle(connection->process);
				assert(status == 0);
				status = sys_close_handle(connection->control);
				assert(status == 0);
				status = sys_close_handle(connection->event);
				assert(status == 0);

				for (size_t j = 0; j < WINDOW_TO_INFO->size();) {
					auto& window_info = (*WINDOW_TO_INFO)[j];
					if (window_info.connection == connection.get()) {
						destroy_window(desktop, window_info.window);
					}
					else {
						++j;
					}
				}

				CONNECTIONS->erase(&connection);
				continue;
			}

			// todo check status and size

			protocol::Response resp {};

			switch (req.type) {
				case protocol::Request::CreateWindow:
				{
					auto window = std::make_unique<DesktopWindow>(false);
					window->set_pos(req.create_window.x, req.create_window.y);
					window->set_size(req.create_window.width, req.create_window.height);

					// todo check errors
					CrescentHandle fb_shared_mem_handle;
					sys_shared_mem_alloc(fb_shared_mem_handle, window->rect.width * window->rect.height * 4);
					CrescentHandle fb_shareable_handle;
					sys_shared_mem_share(fb_shared_mem_handle, connection->process, fb_shareable_handle);
					sys_shared_mem_map(fb_shared_mem_handle, reinterpret_cast<void**>(&window->fb));
					memset(window->fb, 0, window->rect.width * window->rect.height * 4);
					sys_close_handle(fb_shared_mem_handle);

					auto* window_ptr = window.get();

					desktop.add_child(std::move(window));

					desktop.gui.ctx.dirty_rects.push_back({
						.x = req.create_window.x,
						.y = req.create_window.y,
						.width = req.create_window.width + BORDER_WIDTH * 2,
						.height = req.create_window.height + TITLEBAR_HEIGHT + BORDER_WIDTH
					});

					resp.type = protocol::Response::WindowCreated;
					// todo make this an opaque handle instead so it can be verified
					resp.window_created.window_handle = window_ptr;
					resp.window_created.fb_handle = fb_shareable_handle;

					WINDOW_TO_INFO->push_back({
						.window = window_ptr,
						.connection = connection.get(),
						.event_pipe = connection->event
					});

					// todo check status
					while (sys_socket_send(connection->control, &resp, sizeof(resp)) == ERR_TRY_AGAIN);

					break;
				}
				case protocol::Request::CloseWindow:
				{
					auto* window = static_cast<DesktopWindow*>(req.close_window.window_handle);
					destroy_window(desktop, window);

					// todo check status
					while (sys_socket_send(connection->control, &resp, sizeof(resp)) == ERR_TRY_AGAIN);

					break;
				}
				case protocol::Request::Redraw:
				{
					auto* window = static_cast<DesktopWindow*>(req.close_window.window_handle);
					ctx.dirty_rects.push_back(window->get_abs_rect());

					resp.ack.window_handle = window;
					// todo check status
					while (sys_socket_send(connection->control, &resp, sizeof(resp)) == ERR_TRY_AGAIN);

					break;
				}
			}

			++i;
		}

		InputEvent event;
		if (sys_poll_event(event, US_IN_S / 144) == ERR_TRY_AGAIN) {

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
			else {
				desktop.handle_keyboard({
					.code = event.key.code,
					.pressed = event.key.pressed
				});
			}
		}

		uint64_t time_now_us;
		sys_get_time(&time_now_us);
		if (time_now_us - last_time_update_us >= US_IN_S * 60) {
			desktop.taskbar->update_time(desktop.gui.ctx);
			last_time_update_us = time_now_us;
		}

		if (double_buffer) {
			memcpy(back_mapping, front_mapping, info.height * info.pitch);

			desktop.draw();

			fb_link.op = FbLinkOp::Flip;
			status = sys_devlink(link);
			if (status != 0) {
				puts("failed to flip fb");
				return 1;
			}

			auto tmp = back_mapping;
			back_mapping = front_mapping;
			front_mapping = tmp;
			ctx.fb = static_cast<uint32_t*>(back_mapping);
		}
		else {
			desktop.draw();
		}
	}
}
