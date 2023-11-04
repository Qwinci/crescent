#include "ps2.h"
#include "arch/x86/dev/io.h"
#include "dev/timer.h"
#include "ps2_kb.h"
#include "stdio.h"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_CMD_PORT 0x64

u8 ps2_data_read() {
	return in1(PS2_DATA_PORT);
}

void ps2_data_write(u8 data) {
	out1(PS2_DATA_PORT, data);
}

#define STATUS_OUTPUT_FULL (1 << 0)
#define STATUS_INPUT_FULL (1 << 1)
#define STATUS_SYS (1 << 2)
#define STATUS_CMD_CONTROLLER (1 << 3)
#define STATUS_TIMEOUT (1 << 6)
#define STATUS_PARITY (1 << 7)

#define CONF_INT_PORT1 (1 << 0)
#define CONF_INT_PORT2 (1 << 1)
#define CONF_SYS (1 << 2)
#define CONF_PORT1_CLOCK (1 << 4)
#define CONF_PORT2_CLOCK (1 << 5)
#define CONF_PORT1_TRANSLATION (1 << 6)

#define CMD_READ_CONF0 0x20
#define CMD_WRITE_CONF0 0x60
#define CMD_DISABLE_PORT2 0xA7
#define CMD_ENABLE_PORT2 0xA8
#define CMD_TEST_PORT2 0xA9
#define CMD_TEST_CONTROLLER 0xAA
#define CMD_TEST_PORT1 0xAB
#define CMD_DISABLE_PORT1 0xAD
#define CMD_ENABLE_PORT1 0xAE
#define CMD_WRITE_PORT2 0xD4

u8 ps2_status_read() {
	return in1(PS2_STATUS_PORT);
}

static void ps2_cmd_write(u8 data) {
	out1(PS2_CMD_PORT, data);
}

bool ps2_wait_for_output() {
	usize time = arch_get_ns_since_boot();
	while (!(ps2_status_read() & STATUS_OUTPUT_FULL)) {
		if (arch_get_ns_since_boot() >= time + NS_IN_US * 5) {
			return false;
		}
	}
	return true;
}

static bool wait_for_completion() {
	usize time = arch_get_ns_since_boot();
	while (ps2_status_read() & STATUS_INPUT_FULL) {
		if (arch_get_ns_since_boot() >= time + NS_IN_US * 5) {
			return false;
		}
	}
	return true;
}

#define IDENTIFY 0xF2
#define ACK 0xFA
#define RESEND 0xFE
#define DEV_RESET 0xFF

bool ps2_data2_write(u8 data) {
	ps2_cmd_write(CMD_WRITE_PORT2);
	if (!wait_for_completion()) {
		return false;
	}
	ps2_data_write(data);
	if (!wait_for_completion()) {
		return false;
	}
	return true;
}

static bool ps2_disable(bool second) {
	ps2_cmd_write(second ? CMD_DISABLE_PORT2 : CMD_DISABLE_PORT1);
	return wait_for_completion();
}

static bool ps2_enable(bool second) {
	ps2_cmd_write(second ? CMD_ENABLE_PORT2 : CMD_ENABLE_PORT1);
	return wait_for_completion();
}

static void ps2_flush_output() {
	ps2_data_read();
	ps2_data_read();
}

static bool ps2_read_conf(u8* conf) {
	ps2_cmd_write(CMD_READ_CONF0);
	if (!ps2_wait_for_output()) {
		return false;
	}
	*conf = ps2_data_read();
	return true;
}

static bool ps2_write_conf(u8 conf) {
	ps2_cmd_write(CMD_WRITE_CONF0);
	if (!wait_for_completion()) {
		return false;
	}
	ps2_data_write(conf);
	if (!wait_for_completion()) {
		return false;
	}
	return true;
}

static bool ps2_test_controller() {
	ps2_cmd_write(CMD_TEST_CONTROLLER);
	if (!ps2_wait_for_output()) {
		return false;
	}
	return ps2_data_read() == 0x55;
}

static bool ps2_test_port(bool second) {
	ps2_cmd_write(second ? CMD_TEST_PORT2 : CMD_TEST_PORT1);
	if (!ps2_wait_for_output()) {
		return false;
	}
	return ps2_data_read() == 0;
}

static bool ps2_reset(bool second) {
	if (second) {
		return ps2_data2_write(DEV_RESET);
	}
	else {
		ps2_data_write(DEV_RESET);
		if (!wait_for_completion()) {
			return false;
		}
		return true;
	}
}

static void ps2_controller_fail(const char* msg) {
	kprintf("[kernel][x86]: failed to initialize ps2 controller (%s)\n", msg);
}

#define CHECK(expr, msg) do { if (!(expr)) {ps2_controller_fail(msg); return;} } while (0)

static void ps2_start_driver(bool second, bool first_read, const u8 bytes[2]) {
	if (
		!first_read ||
		(bytes[0] == 0xAB &&
		 (bytes[1] == 0x83 || bytes[1] == 0xC1 ||
		  bytes[1] == 0x84 || bytes[1] == 0x85 ||
		  bytes[1] == 0x86 || bytes[1] == 0x90 ||
		  bytes[1] == 0x91 || bytes[1] == 0x92)) ||
		(bytes[0] == 0xAC && bytes[1] == 0xA1)) {
		ps2_kb_init(second);
	}
}

u8 ps2_read() {
	while (!(in1(PS2_STATUS_PORT) & STATUS_OUTPUT_FULL));
	return in1(PS2_DATA_PORT);
}

void ps2_write(u16 port, u8 value) {
	while (in1(PS2_STATUS_PORT) & STATUS_INPUT_FULL);
	out1(port, value);
}

u8 ps2_read_config() {
	ps2_write(PS2_CMD_PORT, CMD_READ_CONF0);
	return ps2_data_read();
}

void ps2_write_config(u8 value) {
	ps2_write(PS2_CMD_PORT, CMD_WRITE_CONF0);
	ps2_write(PS2_DATA_PORT, value);
}

void ps2_init() {
	kprintf("[kernel][x86]: ps2 init\n");

	ps2_write(PS2_CMD_PORT, CMD_DISABLE_PORT1);
	ps2_write(PS2_CMD_PORT, CMD_DISABLE_PORT2);

	u8 config = ps2_read_config();
	config |= CONF_INT_PORT1;
	config &= ~CONF_PORT1_TRANSLATION;

	if (config & CONF_PORT2_CLOCK) {
		config |= CONF_INT_PORT2;
	}

	ps2_write_config(config);

	ps2_write(PS2_CMD_PORT, CMD_ENABLE_PORT1);

	if (config & CONF_PORT2_CLOCK) {
		ps2_write(PS2_CMD_PORT, CMD_ENABLE_PORT2);
	}

	ps2_kb_init(false);

	/*// Disable ports
	CHECK(ps2_disable(false), "disabling first port timed out");
	CHECK(ps2_disable(true), "disabling second port timed out");

	// Flush output
	ps2_flush_output();

	// Disable interrupts for ports and disable translation
	u8 conf;
	CHECK(ps2_read_conf(&conf), "reading conf timed out");
	conf &= ~CONF_INT_PORT1;
	conf &= ~CONF_INT_PORT2;
	conf &= ~CONF_PORT1_TRANSLATION;
	CHECK(ps2_write_conf(conf), "writing conf timed out");

	// Test controller
	if (!ps2_test_controller()) {
		kprintf("[kernel][x86]: ps2 controller self-test failed or timed out\n");
		return;
	}
	else {
		CHECK(ps2_write_conf(conf), "writing conf timed out");
	}

	bool double_channel = conf & 1 << 5;

	// Determine if its double channel or not
	if (double_channel) {
		CHECK(ps2_enable(true), "enabling second port timed out");
		u8 conf2;
		CHECK(ps2_read_conf(&conf2), "reading conf timed out");
		double_channel = !(conf2 & 1 << 5);
		CHECK(ps2_disable(true), "disabling second port timed out");
	}

	bool first_good = false;
	bool second_good = false;

	first_good = ps2_test_port(false);
	if (double_channel) {
		second_good = ps2_test_port(true);
	}

	if (!first_good && !second_good) {
		kprintf("[kernel][x86]: no ps2 devices found\n");
		return;
	}

	if (first_good) {
		conf |= CONF_INT_PORT1;
	}
	if (second_good) {
		conf |= CONF_INT_PORT2;
	}

	CHECK(ps2_write_conf(conf), "writing conf timed out");

	if (first_good) {
		ps2_enable(false);

		ps2_data_write(PS2_DISABLE_SCANNING);
		if (!ps2_wait_for_output()) {
			kprintf("[kernel][x86]: first ps2 port timed out\n");
			goto identify_second;
		}
		if (ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: first ps2 port didn't ack disable scanning\n");
			goto identify_second;
		}

		ps2_data_write(IDENTIFY);
		if (!ps2_wait_for_output()) {
			kprintf("[kernel][x86]: first ps2 port timed out\n");
			goto identify_second;
		}
		if (ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: first ps2 port didn't ack identify\n");
			goto identify_second;
		}

		usize time = arch_get_ns_since_boot();
		u8 bytes[2] = {};
		bool first_read = false;
		while (arch_get_ns_since_boot() < time + NS_IN_US * 5) {
			if (ps2_status_read() & STATUS_OUTPUT_FULL) {
				if (first_read) {
					bytes[1] = ps2_data_read();
					break;
				}
				else {
					bytes[0] = ps2_data_read();
					first_read = true;
				}
			}
		}

		kprintf("[kernel][x86]: first ps2 device %x %x\n", bytes[0], bytes[1]);

		ps2_start_driver(false, first_read, bytes);
	}

identify_second:
	if (second_good) {
		ps2_enable(true);

		if (!ps2_data2_write(PS2_DISABLE_SCANNING) || !ps2_wait_for_output()) {
			kprintf("[kernel][x86]: second ps2 port timed out\n");
			return;
		}
		if (ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: second ps2 port didn't ack disable scanning\n");
			return;
		}

		if (!ps2_data2_write(IDENTIFY) || !ps2_wait_for_output()) {
			kprintf("[kernel][x86]: first ps2 port timed out\n");
			return;
		}
		if (ps2_data_read() != 0xFA) {
			kprintf("[kernel][x86]: second ps2 port didn't ack identify\n");
			return;
		}

		usize time = arch_get_ns_since_boot();
		u8 bytes[2] = {};
		bool first_read = false;
		while (arch_get_ns_since_boot() < time + NS_IN_US * 5) {
			if (ps2_status_read() & STATUS_OUTPUT_FULL) {
				if (first_read) {
					bytes[1] = ps2_data_read();
					break;
				}
				else {
					bytes[0] = ps2_data_read();
					first_read = true;
				}
			}
		}

		kprintf("[kernel][x86]: second ps2 device %x %x\n", bytes[0], bytes[1]);

		ps2_start_driver(true, first_read, bytes);
	}*/
}