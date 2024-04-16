#include "arch/irq.hpp"
#include "assert.hpp"
#include "io_apic.hpp"
#include "manually_destroy.hpp"
#include "ps2.hpp"
#include "sys/event_queue.hpp"
#include "x86/irq.hpp"

enum class PacketType {
	Generic,
	ZAxis
};

namespace {
	PacketType PACKET_TYPE {};
}

static bool handle_irq(IrqFrame*) {
	if (!ps2_has_data()) {
		return true;
	}

	u8 byte0 = ps2_receive_data();
	u8 byte1 = ps2_receive_data();
	u8 byte2 = ps2_receive_data();

	bool left_pressed = byte0 & 1;
	bool right_pressed = byte0 >> 1 & 1;
	bool middle_pressed = byte0 >> 2 & 1;
	bool negative_x = byte0 >> 4 & 1;
	bool negative_y = byte0 >> 5 & 1;

	i16 x_movement = static_cast<i16>(negative_x ? (byte1 - 0x100) : byte1);
	i16 y_movement = static_cast<i16>(negative_y ? (byte2 - 0x100) : byte2);
	i8 z_movement = 0;
	switch (PACKET_TYPE) {
		case PacketType::Generic:
			break;
		case PacketType::ZAxis:
			z_movement = static_cast<i8>(ps2_receive_data());
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

	return true;
}

static ManuallyDestroy<IrqHandler> IRQ_HANDLER {
	IrqHandler {
		.fn = handle_irq,
		.can_be_shared = false
	}
};

namespace cmd {
	static constexpr u8 SAMPLE_RATE = 0xF3;
}

void ps2_mouse_init(bool second) {
	println("[kernel][x86]: ps2 mouse init");

	ps2_send_data2(second, cmd::SAMPLE_RATE, 200);
	ps2_send_data2(second, cmd::SAMPLE_RATE, 100);
	ps2_send_data2(second, cmd::SAMPLE_RATE, 80);
	auto res = ps2_identify(second);
	bool has_z_axis = res[0] == 3;
	bool has_5_buttons = false;
	if (has_z_axis) {
		/*ps2_send_data2(second, cmd::SAMPLE_RATE, 200);
		ps2_send_data2(second, cmd::SAMPLE_RATE, 200);
		ps2_send_data2(second, cmd::SAMPLE_RATE, 80);
		res = ps2_identify(second);
		has_5_buttons = res[0] == 4;

		if (has_5_buttons) {
			PACKET_TYPE = PacketType::MultipleButtons;
		}
		else {
			PACKET_TYPE = PacketType::ZAxis;
		}*/
		PACKET_TYPE = PacketType::ZAxis;
	}
	else {
		PACKET_TYPE = PacketType::Generic;
	}

	println("[kernel][x86]: has z axis: ", has_z_axis, " has 5 buttons: ", has_5_buttons);

	u32 irq = x86_alloc_irq(1, false);
	assert(irq);
	register_irq_handler(irq, &*IRQ_HANDLER);

	if (!second) {
		IO_APIC.register_isa_irq(1, irq);
	}
	else {
		IO_APIC.register_isa_irq(12, irq);
	}
}
