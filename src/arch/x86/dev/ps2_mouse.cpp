#include "assert.hpp"
#include "dev/clock.hpp"
#include "ps2.hpp"
#include "sys/event_queue.hpp"

enum class PacketType {
	Generic,
	ZAxis
};

namespace cmd {
	static constexpr Ps2Cmd SAMPLE_RATE {
		.cmd = 0xF3,
		.params = 1,
		.ret = 0
	};
}

struct Ps2Mouse : Ps2Device {
	void add_event() const {
		bool left_pressed = flags & 1;
		bool right_pressed = flags >> 1 & 1;
		bool middle_pressed = flags >> 2 & 1;
		bool negative_x = flags >> 4 & 1;
		bool negative_y = flags >> 5 & 1;

		i16 x_movement = static_cast<i16>(negative_x ? (x - 0x100) : x);
		i16 y_movement = static_cast<i16>(negative_y ? (y - 0x100) : y);
		i8 z_movement = 0;
		switch (packet_type) {
			case PacketType::Generic:
				break;
			case PacketType::ZAxis:
				z_movement = static_cast<i8>(z);
				break;
		}

		GLOBAL_EVENT_QUEUE.push({
			.type = EVENT_TYPE_MOUSE,
			.mouse {
				.x_movement = x_movement,
				.y_movement = y_movement,
				.z_movement = z_movement,
				.left_pressed = left_pressed,
				.right_pressed = right_pressed,
				.middle_pressed = middle_pressed
			}
		});
	}

	void on_receive(u8 byte) override {
		if (!scanning) {
			return;
		}

		switch (state) {
			case State::Flags:
				flags = byte;
				state = State::X;
				break;
			case State::X:
				x = byte;
				state = State::Y;
				break;
			case State::Y:
				y = byte;
				if (packet_type == PacketType::Generic) {
					state = State::Flags;
					add_event();
				}
				else {
					state = State::Z;
				}
				break;
			case State::Z:
				z = byte;
				state = State::Flags;
				add_event();
				break;
		}
	}

	void resume() override {
		if (!probe()) {
			auto guard = port->lock.lock();
			port->device = nullptr;
			return;
		}

		scanning = false;

		println("[kernel][x86]: ps2 mouse resume");

		u8 param[2] {};
		param[0] = 200;
		port->send_cmd(cmd::SAMPLE_RATE, param);
		param[0] = 100;
		port->send_cmd(cmd::SAMPLE_RATE, param);
		param[0] = 80;
		port->send_cmd(cmd::SAMPLE_RATE, param);

		port->send_cmd(ps2_cmd::GET_ID, param);
		bool has_z_axis = param[0] == 3;

		if (has_z_axis) {
			packet_type = PacketType::ZAxis;
		}
		else {
			packet_type = PacketType::Generic;
		}

		param[0] = 200;
		port->send_cmd(cmd::SAMPLE_RATE, param);

		{
			auto guard = port->lock.lock();
			scanning = true;
		}

		port->send_cmd(ps2_cmd::ENABLE_SCANNING, nullptr);
	}

	[[nodiscard]] bool probe() const {
		u8 param[2] {0xA5};

		if (!port->send_cmd(ps2_cmd::GET_ID, param)) {
			return false;
		}

		if (param[0] != 0 && param[0] != 3 && param[0] != 4 && param[0] != 0xFF) {
			return false;
		}

		port->send_cmd(ps2_cmd::SET_DEFAULTS, nullptr);
		return true;
	}

	Ps2Port* port {};

	PacketType packet_type {};

	enum class State {
		Flags,
		X,
		Y,
		Z
	} state {};

	u8 flags = 0;
	u8 x = 0;
	u8 y = 0;
	u8 z = 0;

	bool scanning {};
};

bool ps2_mouse_init(Ps2Port* port) {
	auto* mouse = new Ps2Mouse {};
	mouse->port = port;

	{
		auto guard = port->lock.lock();
		port->device = mouse;
	}

	if (!mouse->probe()) {
		auto guard = port->lock.lock();
		port->device = nullptr;
		delete mouse;
		return false;
	}

	println("[kernel][x86]: ps2 mouse init");

	u8 param[2] {};
	param[0] = 200;
	port->send_cmd(cmd::SAMPLE_RATE, param);
	param[0] = 100;
	port->send_cmd(cmd::SAMPLE_RATE, param);
	param[0] = 80;
	port->send_cmd(cmd::SAMPLE_RATE, param);

	port->send_cmd(ps2_cmd::GET_ID, param);
	bool has_z_axis = param[0] == 3;

	if (has_z_axis) {
		mouse->packet_type = PacketType::ZAxis;
	}
	else {
		mouse->packet_type = PacketType::Generic;
	}

	param[0] = 200;
	port->send_cmd(cmd::SAMPLE_RATE, param);

	{
		auto guard = port->lock.lock();
		mouse->scanning = true;
	}

	port->send_cmd(ps2_cmd::ENABLE_SCANNING, nullptr);

	println("[kernel][x86]: ps2 mouse has z axis: ", has_z_axis);

	return true;
}
