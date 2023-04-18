#include "ps2_kb.h"
#include "arch/interrupts.h"
#include "arch/misc.h"
#include "arch/x86/dev/io_apic.h"
#include "crescent/input.h"
#include "layout/fi.h"
#include "ps2.h"
#include "sched/sched.h"
#include "stdio.h"

static OneByteScancodeTranslatorFn translator_fn = NULL;

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
static Modifier ps2_modifiers = MOD_NONE;

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
			key = translator_fn(byte0);
		}

		if (key == SCAN_LEFT_SHIFT || key == SCAN_RIGHT_SHIFT) {
			if (!released) {
				ps2_modifiers |= MOD_SHIFT;
			}
			else {
				ps2_modifiers &= ~MOD_SHIFT;
			}
		}
		if (key == SCAN_RIGHT_ALT) {
			if (!released) {
				ps2_modifiers |= MOD_ALT_GR;
			}
			else {
				ps2_modifiers &= ~MOD_ALT_GR;
			}
		}
		if (key == SCAN_LEFT_ALT) {
			if (!released) {
				ps2_modifiers |= MOD_ALT;
			}
			else {
				ps2_modifiers &= ~MOD_ALT;
			}
		}

		if (ACTIVE_INPUT_TASK) {
			Event event = {
				.type = released ? EVENT_KEY_RELEASE : EVENT_KEY_PRESS,
				.key = key,
				.mods = ps2_modifiers
			};
			event_queue_push(&ACTIVE_INPUT_TASK->event_queue, event);
		}
	}
}

extern u8 X86_BSP_ID;

void ps2_kb_init(bool second) {
	ps2_kb_layout_fi();

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

void ps2_kb_set_translation(OneByteScancodeTranslatorFn fn) {
	translator_fn = fn;
}