#include "ps2_kb.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "arch/x86/dev/io_apic.h"
#include "ps2.h"
#include "sched/sched.h"
#include "stdio.h"
#include "crescent/input.h"

static Scancode scancode_set2_no_prefix_table[] = {
	[0x01] = SCAN_F9,
	[0x03] = SCAN_F5,
	[0x04] = SCAN_F3,
	[0x05] = SCAN_F1,
	[0x06] = SCAN_F2,
	[0x07] = SCAN_F12,
	[0x09] = SCAN_F10,
	[0x0A] = SCAN_F8,
	[0x0B] = SCAN_F6,
	[0x0C] = SCAN_F4,
	[0x0D] = SCAN_TAB,
	[0x0E] = SCAN_GRAVE,
	[0x11] = SCAN_LEFT_ALT,
	[0x12] = SCAN_LEFT_SHIFT,
	[0x14] = SCAN_LEFT_CONTROL,
	[0x15] = SCAN_Q,
	[0x16] = SCAN_1,
	[0x1A] = SCAN_Z,
	[0x1B] = SCAN_S,
	[0x1C] = SCAN_A,
	[0x1D] = SCAN_W,
	[0x1E] = SCAN_2,
	[0x21] = SCAN_C,
	[0x22] = SCAN_X,
	[0x23] = SCAN_D,
	[0x24] = SCAN_E,
	[0x25] = SCAN_4,
	[0x26] = SCAN_3,
	[0x29] = SCAN_SPACE,
	[0x2A] = SCAN_V,
	[0x2B] = SCAN_F,
	[0x2C] = SCAN_T,
	[0x2D] = SCAN_R,
	[0x2E] = SCAN_5,
	[0x31] = SCAN_N,
	[0x32] = SCAN_B,
	[0x33] = SCAN_H,
	[0x34] = SCAN_G,
	[0x35] = SCAN_Y,
	[0x36] = SCAN_6,
	[0x3A] = SCAN_M,
	[0x3B] = SCAN_J,
	[0x3C] = SCAN_U,
	[0x3D] = SCAN_7,
	[0x3E] = SCAN_8,
	[0x41] = SCAN_COMMA,
	[0x42] = SCAN_K,
	[0x43] = SCAN_I,
	[0x44] = SCAN_O,
	[0x45] = SCAN_0,
	[0x46] = SCAN_9,
	[0x49] = SCAN_DOT,
	[0x4A] = SCAN_SLASH,
	[0x4B] = SCAN_L,
	[0x4C] = SCAN_SEMICOLON,
	[0x4D] = SCAN_P,
	[0x4E] = SCAN_MINUS,
	[0x52] = SCAN_APOSTROPHE,
	[0x54] = SCAN_LBRACKET,
	[0x55] = SCAN_EQUAL,
	[0x58] = SCAN_CAPSLOCK,
	[0x59] = SCAN_RIGHT_SHIFT,
	[0x5A] = SCAN_ENTER,
	[0x5B] = SCAN_RBRACKET,
	[0x5D] = SCAN_BACKSLASH,
	[0x66] = SCAN_BACKSPACE,
	[0x69] = SCAN_KEYPAD_1,
	[0x6B] = SCAN_KEYPAD_4,
	[0x6C] = SCAN_KEYPAD_7,
	[0x70] = SCAN_KEYPAD_0,
	[0x71] = SCAN_KEYPAD_DOT,
	[0x72] = SCAN_KEYPAD_2,
	[0x73] = SCAN_KEYPAD_5,
	[0x74] = SCAN_KEYPAD_6,
	[0x75] = SCAN_KEYPAD_8,
	[0x76] = SCAN_ESC,
	[0x77] = SCAN_NUMLK,
	[0x78] = SCAN_F11,
	[0x79] = SCAN_KEYPAD_PLUS,
	[0x7A] = SCAN_KEYPAD_3,
	[0x7B] = SCAN_KEYPAD_MINUS,
	[0x7C] = SCAN_KEYPAD_STAR,
	[0x7D] = SCAN_KEYPAD_9,
	[0x7E] = SCAN_SCRLK,
	[0x83] = SCAN_F7
};

static Scancode ps2_scancode_set2_no_prefix(u8 byte) {
	if (byte < sizeof(scancode_set2_no_prefix_table) / sizeof(*scancode_set2_no_prefix_table)) {
		return scancode_set2_no_prefix_table[byte];
	}
	return SCAN_RESERVED;
}

static Scancode ps2_scancode_set2_e0(u8 byte) {
	switch (byte) {
		case 0x10:
			return SCAN_FIND;
		case 0x11:
			return SCAN_RIGHT_ALT;
		case 0x14:
			return SCAN_RIGHT_CONTROL;
		case 0x15:
			return SCAN_MULTIMEDIA_PREV_TRACK;
		case 0x18:
			return SCAN_MULTIMEDIA_WWW_FAVOURITES;
		case 0x1F:
			return SCAN_LEFT_GUI;
		case 0x20:
			return SCAN_MULTIMEDIA_WWW_REFRESH;
		case 0x21:
			return SCAN_VOL_DOWN;
		case 0x23:
			return SCAN_MUTE;
		case 0x27:
			return SCAN_RIGHT_GUI;
		case 0x28:
			return SCAN_MULTIMEDIA_WWW_STOP;
		case 0x2B:
			return SCAN_MULTIMEDIA_CALC;
		case 0x2F:
			return SCAN_APP;
		case 0x30:
			return SCAN_MULTIMEDIA_WWW_FORWARD;
		case 0x32:
			return SCAN_VOL_UP;
		case 0x34:
			return SCAN_MULTIMEDIA_PAUSE;
		case 0x37:
			return SCAN_POWER;
		case 0x38:
			return SCAN_MULTIMEDIA_WWW_BACK;
		case 0x3A:
			return SCAN_MULTIMEDIA_WWW_HOME;
		case 0x3B:
			return SCAN_STOP;
		case 0x3F:
			return SCAN_SLEEP;
		case 0x40:
			return SCAN_MULTIMEDIA_MY_COMPUTER;
		case 0x48:
			return SCAN_MULTIMEDIA_EMAIL;
		case 0x4A:
			return SCAN_KEYPAD_SLASH;
		case 0x4D:
			return SCAN_MULTIMEDIA_NEXT_TRACK;
		case 0x50:
			return SCAN_MULTIMEDIA_SELECT;
		case 0x5A:
			return SCAN_KEYPAD_ENTER;
		case 0x5E:
			return SCAN_WAKE;
		case 0x69:
			return SCAN_END;
		case 0x6B:
			return SCAN_LEFT;
		case 0x6C:
			return SCAN_HOME;
		case 0x70:
			return SCAN_INSERT;
		case 0x71:
			return SCAN_DELETE;
		case 0x72:
			return SCAN_DOWN;
		case 0x74:
			return SCAN_RIGHT;
		case 0x75:
			return SCAN_UP;
		case 0x7D:
			return SCAN_PGUP;
		default:
			return SCAN_RESERVED;
	}
}

static Scancode ps2_scancode_set2_e1(u8 byte1, u8 byte2) {
	if (byte1 == 0x14 && byte2 == 0x77) {
		return SCAN_PAUSE;
	}
	else {
		return SCAN_RESERVED;
	}
}

#define PS2_QUEUE_SIZE 128
static u8 ps2_key_queue[PS2_QUEUE_SIZE] = {};
static usize ps2_key_queue_int_ptr = 0;
static usize ps2_key_queue_translator_ptr = 0;
static usize ps2_key_queue_size = 0;
static Task* ps2_translator_task = NULL;

static bool ps2_kb_handler(void*, void*) {
	if (!(ps2_status_read() & PS2_STATUS_OUTPUT_FULL)) {
		return false;
	}

	if (ps2_key_queue_size == PS2_QUEUE_SIZE) {
		kprintf("WARNING: ps2 key queue overflow\n");
		ps2_data_read();
		return true;
	}
	ps2_key_queue_size++;
	ps2_key_queue[ps2_key_queue_int_ptr] = ps2_data_read();
	ps2_key_queue_int_ptr = (ps2_key_queue_int_ptr + 1) % PS2_QUEUE_SIZE;
	sched_unblock(ps2_translator_task);
	return true;
}

static u8 queue_get_byte() {
	// todo spl
	void* flags = enter_critical();
	u8 byte;
	if (ps2_key_queue_size) {
		byte = ps2_key_queue[ps2_key_queue_translator_ptr];
		ps2_key_queue_translator_ptr = (ps2_key_queue_translator_ptr + 1) % PS2_QUEUE_SIZE;
	}
	else {
		sched_block(TASK_STATUS_WAITING);
		sched();
		byte = ps2_key_queue[ps2_key_queue_translator_ptr];
		ps2_key_queue_translator_ptr = (ps2_key_queue_translator_ptr + 1) % PS2_QUEUE_SIZE;
	}
	--ps2_key_queue_size;
	leave_critical(flags);
	return byte;
}

NORETURN static void ps2_kb_translator() {
	while (true) {
		u8 byte0 = queue_get_byte();

		bool released = false;
		Scancode key;

		if (byte0 == 0xE0) {
			u8 byte1 = queue_get_byte();
			if (byte1 == 0xF0) {
				released = true;
				byte1 = queue_get_byte();
			}
			key = ps2_scancode_set2_e0(byte1);
		}
		else if (byte0 == 0xE1) {
			u8 byte1 = queue_get_byte();
			if (byte1 == 0xF0) {
				released = true;
				byte1 = queue_get_byte();
			}
			u8 byte2 = queue_get_byte();
			if (byte2 == 0xF0) {
				released = true;
				byte2 = queue_get_byte();
			}
			key = ps2_scancode_set2_e1(byte1, byte2);
		}
		else {
			if (byte0 == 0xF0) {
				released = true;
				byte0 = queue_get_byte();
			}
			key = ps2_scancode_set2_no_prefix(byte0);
		}

		if (ACTIVE_INPUT_TASK) {
			Event event = {
				.type = released ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS,
				.key = key
			};
			event_queue_push(&ACTIVE_INPUT_TASK->event_queue, event);
		}
	}
}

extern u8 X86_BSP_ID;

void ps2_kb_init(bool second) {
	if (!second) {
		ps2_data_write(PS2_ENABLE_SCANNING);
		if (!ps2_wait_for_output() || ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: enabling scanning for the first ps2 port timed out\n");
			return;
		}
	}
	else {
		if (!ps2_data2_write(PS2_ENABLE_SCANNING) || ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: enabling scanning for the second ps2 port timed out\n");
			return;
		}
	}

	u8 i = arch_alloc_int(ps2_kb_handler, NULL);
	if (!i) {
		kprintf("[kernel][x86]: failed to allocate interrupt for ps2 keyboard\n");
		return;
	}
	IoApicRedirEntry entry = {
		.vec = i,
		.dest = X86_BSP_ID
	};
	if (!second) {
		io_apic_register_isa_irq(1, entry);
	}
	else {
		io_apic_register_isa_irq(12, entry);
	}

	ps2_translator_task = arch_create_kernel_task("ps2 translator", ps2_kb_translator, NULL);
	ps2_translator_task->pin_level = true;
	void* flags = enter_critical();
	sched_queue_task(ps2_translator_task);
	leave_critical(flags);
}