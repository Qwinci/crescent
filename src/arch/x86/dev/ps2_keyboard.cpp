#include "arch/irq.hpp"
#include "assert.hpp"
#include "crescent/event.h"
#include "io_apic.hpp"
#include "manually_destroy.hpp"
#include "ps2.hpp"
#include "sys/event_queue.hpp"
#include "x86/irq.hpp"

namespace cmd {
	static constexpr u8 TYPEMATIC_BYTE = 0xF3;
}

static Scancode ps2_one_byte(u8 byte) {
	switch (byte) {
		case 0x1:
			return SCANCODE_F9;
		case 0x3:
			return SCANCODE_F5;
		case 0x4:
			return SCANCODE_F3;
		case 0x5:
			return SCANCODE_F1;
		case 0x6:
			return SCANCODE_F2;
		case 0x7:
			return SCANCODE_F12;
		case 0x9:
			return SCANCODE_F10;
		case 0xA:
			return SCANCODE_F8;
		case 0xB:
			return SCANCODE_F6;
		case 0xC:
			return SCANCODE_F4;
		case 0xD:
			return SCANCODE_TAB;
		case 0xE:
			return SCANCODE_GRAVE;
		case 0x11:
			return SCANCODE_LALT;
		case 0x12:
			return SCANCODE_LSHIFT;
		case 0x14:
			return SCANCODE_LCTRL;
		case 0x15:
			return SCANCODE_Q;
		case 0x16:
			return SCANCODE_1;
		case 0x1A:
			return SCANCODE_Z;
		case 0x1B:
			return SCANCODE_S;
		case 0x1C:
			return SCANCODE_A;
		case 0x1D:
			return SCANCODE_W;
		case 0x1E:
			return SCANCODE_2;
		case 0x21:
			return SCANCODE_C;
		case 0x22:
			return SCANCODE_X;
		case 0x23:
			return SCANCODE_D;
		case 0x24:
			return SCANCODE_E;
		case 0x25:
			return SCANCODE_4;
		case 0x26:
			return SCANCODE_3;
		case 0x29:
			return SCANCODE_SPACE;
		case 0x2A:
			return SCANCODE_V;
		case 0x2B:
			return SCANCODE_F;
		case 0x2C:
			return SCANCODE_T;
		case 0x2D:
			return SCANCODE_R;
		case 0x2E:
			return SCANCODE_5;
		case 0x31:
			return SCANCODE_N;
		case 0x32:
			return SCANCODE_B;
		case 0x33:
			return SCANCODE_H;
		case 0x34:
			return SCANCODE_G;
		case 0x35:
			return SCANCODE_Y;
		case 0x36:
			return SCANCODE_6;
		case 0x3A:
			return SCANCODE_M;
		case 0x3B:
			return SCANCODE_J;
		case 0x3C:
			return SCANCODE_U;
		case 0x3D:
			return SCANCODE_7;
		case 0x3E:
			return SCANCODE_8;
		case 0x41:
			return SCANCODE_COMMA;
		case 0x42:
			return SCANCODE_K;
		case 0x43:
			return SCANCODE_I;
		case 0x44:
			return SCANCODE_O;
		case 0x45:
			return SCANCODE_0;
		case 0x46:
			return SCANCODE_9;
		case 0x49:
			return SCANCODE_PERIOD;
		case 0x4A:
			return SCANCODE_SLASH;
		case 0x4B:
			return SCANCODE_L;
		case 0x4C:
			return SCANCODE_SEMICOLON;
		case 0x4D:
			return SCANCODE_P;
		case 0x4E:
			return SCANCODE_MINUS;
		case 0x52:
			return SCANCODE_APOSTROPHE;
		case 0x54:
			return SCANCODE_LEFT_BRACKET;
		case 0x55:
			return SCANCODE_EQUALS;
		case 0x58:
			return SCANCODE_CAPSLOCK;
		case 0x59:
			return SCANCODE_RSHIFT;
		case 0x5A:
			return SCANCODE_RETURN;
		case 0x5B:
			return SCANCODE_RIGHT_BRACKET;
		case 0x5D:
			return SCANCODE_BACKSLASH;
		case 0x66:
			return SCANCODE_BACKSPACE;
		case 0x69:
			return SCANCODE_KP_1;
		case 0x6C:
			return SCANCODE_KP_7;
		case 0x70:
			return SCANCODE_KP_0;
		case 0x71:
			return SCANCODE_KP_PERIOD;
		case 0x72:
			return SCANCODE_KP_2;
		case 0x73:
			return SCANCODE_KP_5;
		case 0x74:
			return SCANCODE_KP_6;
		case 0x75:
			return SCANCODE_KP_8;
		case 0x76:
			return SCANCODE_ESCAPE;
		case 0x77:
			return SCANCODE_NUM_LOCK;
		case 0x78:
			return SCANCODE_F11;
		case 0x79:
			return SCANCODE_KP_PLUS;
		case 0x7A:
			return SCANCODE_KP_3;
		case 0x7B:
			return SCANCODE_KP_MINUS;
		case 0x7C:
			return SCANCODE_KP_MULTIPLY;
		case 0x7D:
			return SCANCODE_KP_9;
		case 0x7E:
			return SCANCODE_SCROLL_LOCK;
		case 0x83:
			return SCANCODE_F7;
		default:
			break;
	}
	return SCANCODE_UNKNOWN;
}

static Scancode ps2_e0(u8 byte) {
	switch (byte) {
		case 0x10:
			return SCANCODE_AC_SEARCH;
		case 0x11:
			return SCANCODE_RALT;
		case 0x14:
			return SCANCODE_RCTRL;
		case 0x15:
			return SCANCODE_AUDIO_PREV;
		case 0x18:
			return SCANCODE_AC_BOOKMARKS;
		case 0x1F:
			return SCANCODE_LGUI;
		case 0x20:
			return SCANCODE_AC_REFRESH;
		case 0x21:
			return SCANCODE_VOLUME_DOWN;
		case 0x23:
			return SCANCODE_AUDIO_MUTE;
		case 0x27:
			return SCANCODE_RGUI;
		case 0x28:
			return SCANCODE_AC_STOP;
		case 0x2B:
			return SCANCODE_CALCULATOR;
		case 0x2F:
			return SCANCODE_APPLICATION;
		case 0x30:
			return SCANCODE_AC_FORWARD;
		case 0x32:
			return SCANCODE_VOLUME_UP;
		case 0x34:
			return SCANCODE_AUDIO_PLAY;
		case 0x38:
			return SCANCODE_AC_BACK;
		case 0x3A:
			return SCANCODE_AC_HOME;
		case 0x3B:
			return SCANCODE_AUDIO_STOP;
		case 0x40:
			return SCANCODE_COMPUTER;
		case 0x48:
			return SCANCODE_MAIL;
		case 0x4A:
			return SCANCODE_KP_DIVIDE;
		case 0x4D:
			return SCANCODE_AUDIO_NEXT;
		case 0x50:
			return SCANCODE_MEDIA_SELECT;
		case 0x5A:
			return SCANCODE_KP_ENTER;
		case 0x69:
			return SCANCODE_END;
		case 0x6B:
			return SCANCODE_LEFT;
		case 0x6C:
			return SCANCODE_HOME;
		case 0x70:
			return SCANCODE_INSERT;
		case 0x71:
			return SCANCODE_DELETE;
		case 0x72:
			return SCANCODE_DOWN;
		case 0x74:
			return SCANCODE_RIGHT;
		case 0x75:
			return SCANCODE_UP;
		case 0x7A:
			return SCANCODE_PAGE_DOWN;
		case 0x7D:
			return SCANCODE_PAGE_UP;
		default:
			break;
	}
	return SCANCODE_UNKNOWN;
}

namespace {
	enum class State {
		None,
		E0,
		E1,
		E1_14
	} STATE {};
	bool PRESSED = true;
}

static bool handle_irq(IrqFrame*) {
	if (!ps2_has_data()) {
		return false;
	}

	u8 byte = ps2_receive_data();
	if (byte == 0xE0) {
		STATE = State::E0;
	}
	else if (byte == 0xE1) {
		STATE = State::E1;
	}
	else if (byte == 0xF0) {
		PRESSED = false;
	}
	else {
		Scancode code;

		switch (STATE) {
			case State::None:
				code = ps2_one_byte(byte);
				break;
			case State::E0:
				code = ps2_e0(byte);
				STATE = State::None;
				break;
			case State::E1:
				if (byte == 0x14) {
					STATE = State::E1_14;
					return true;
				}
				else {
					STATE = State::None;
					PRESSED = true;
					return true;
				}
			case State::E1_14:
				if (byte == 0x77) {
					STATE = State::None;
					code = SCANCODE_PAUSE;
					break;
				}
				else {
					STATE = State::None;
					PRESSED = true;
					return true;
				}
		}

		GLOBAL_EVENT_QUEUE.push({
			.type = EVENT_TYPE_KEY,
			.key {
				.code = code,
				.pressed = PRESSED
			}
		});

		PRESSED = true;
	}

	return true;
}

static ManuallyDestroy<IrqHandler> IRQ_HANDLER {
	IrqHandler {
		.fn = handle_irq,
		.can_be_shared = false
	}
};

void ps2_keyboard_init(bool second) {
	println("[kernel][x86]: ps2 keyboard init");

	u32 irq = x86_alloc_irq(1, false);
	assert(irq);
	register_irq_handler(irq, &*IRQ_HANDLER);

	if (!second) {
		IO_APIC.register_isa_irq(1, irq);
	}
	else {
		IO_APIC.register_isa_irq(12, irq);
	}

	ps2_send_data2(second, cmd::TYPEMATIC_BYTE, 0);
}
