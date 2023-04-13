#include "ps2.h"
#include "arch/x86/dev/io.h"
#include "dev/timer.h"
#include "stdio.h"

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_CMD_PORT 0x64

static u8 ps2_data_read() {
	return in1(PS2_DATA_PORT);
}

static void ps2_data_write(u8 data) {
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

static u8 ps2_status_read() {
	return in1(PS2_STATUS_PORT);
}

static void ps2_cmd_write(u8 data) {
	out1(PS2_CMD_PORT, data);
}

static void wait_for_output() {
	io_wait();
	while (!(ps2_status_read() & STATUS_OUTPUT_FULL)) {
		io_wait();
	}
}

static void wait_for_completion() {
	io_wait();
	while (ps2_status_read() & STATUS_INPUT_FULL) {
		io_wait();
	}
}

#define DEV_RESET 0xFF
#define ACK 0xFA
#define RESEND 0xFE

void ps2_init() {
	kprintf("[kernel][x86]: ps2 init\n");
	ps2_cmd_write(CMD_DISABLE_PORT1);
	wait_for_completion();
	ps2_cmd_write(CMD_DISABLE_PORT2);
	wait_for_completion();
	while (ps2_status_read() & STATUS_OUTPUT_FULL) {
		ps2_data_read();
	}

	ps2_cmd_write(CMD_READ_CONF0);
	wait_for_output();
	u8 conf = ps2_data_read();

	conf &= ~CONF_INT_PORT1;
	conf &= ~CONF_INT_PORT2;
	conf &= ~CONF_PORT1_TRANSLATION;

	ps2_cmd_write(CMD_WRITE_CONF0);
	wait_for_completion();
	ps2_data_write(conf);
	wait_for_completion();

	bool dual_channel = conf & 1 << 5;

	ps2_cmd_write(CMD_TEST_CONTROLLER);
	wait_for_output();
	u8 result = ps2_data_read();

	ps2_cmd_write(CMD_WRITE_CONF0);
	wait_for_completion();
	ps2_data_write(conf);
	wait_for_completion();

	if (result != 0x55) {
		kprintf("[kernel][x86]: ps2 controller self-test failed\n");
		return;
	}

	if (dual_channel) {
		ps2_cmd_write(CMD_ENABLE_PORT2);
		wait_for_completion();
		ps2_cmd_write(CMD_READ_CONF0);
		wait_for_output();
		if (ps2_data_read() & 1 << 5) {
			dual_channel = false;
		}
		else {
			ps2_cmd_write(CMD_DISABLE_PORT2);
			wait_for_completion();
		}
	}

	bool first_works = false;
	bool second_works = false;

	ps2_cmd_write(CMD_TEST_PORT1);
	wait_for_output();
	if (ps2_data_read() != 0) {
		kprintf("[kernel][x86]: ps2 port0 test failed\n");
	}
	else {
		first_works = true;
	}

	if (dual_channel) {
		ps2_cmd_write(CMD_TEST_PORT2);
		wait_for_output();
		if (ps2_data_read() != 0) {
			kprintf("[kernel][x86]: ps2 port1 test failed\n");
		}
		else {
			second_works = true;
		}
	}

	if (!first_works && !second_works) {
		kprintf("[kernel][x86]: no working ps2 ports found\n");
		return;
	}

	if (first_works) {
		ps2_cmd_write(CMD_ENABLE_PORT1);
		wait_for_completion();

		resend1:
		ps2_data_write(DEV_RESET);
		usize start = arch_get_ns_since_boot();
		while (!(ps2_status_read() & STATUS_OUTPUT_FULL)) {
			usize time = arch_get_ns_since_boot();
			if (time > start + NS_IN_US * US_IN_MS) {
				kprintf("[kernel][x86]: first ps2 port didn't respond to reset\n");
				first_works = false;
				break;
			}
		}

		u8 res = ps2_data_read();
		if (res == ACK) {
			if (ps2_data_read() != 0xAA) {
				kprintf("[kernel][x86]: first ps2 port self-test failed\n");
				first_works = false;
			}
		}
		else if (res == RESEND) {
			goto resend1;
		}
		else if (res != 0xAA) {
			kprintf("[kernel][x86]: first ps2 port self-test failed\n");
			first_works = false;
		}

		ps2_data_read();

		if (first_works) {
			conf |= CONF_INT_PORT1;
		}
	}
	if (second_works) {
		ps2_cmd_write(CMD_ENABLE_PORT2);
		wait_for_completion();

		resend2:
		ps2_cmd_write(CMD_WRITE_PORT2);
		wait_for_completion();

		ps2_data_write(DEV_RESET);
		usize start = arch_get_ns_since_boot();
		while (!(ps2_status_read() & STATUS_OUTPUT_FULL)) {
			usize time = arch_get_ns_since_boot();
			if (time > start + NS_IN_US * US_IN_MS) {
				kprintf("[kernel][x86]: second ps2 port didn't respond to reset\n");
				second_works = false;
				break;
			}
		}

		u8 res = ps2_data_read();
		if (res == ACK) {
			if (ps2_data_read() != 0xAA) {
				kprintf("[kernel][x86]: second ps2 port self-test failed\n");
				second_works = false;
			}
		}
		else if (res == RESEND) {
			goto resend2;
		}
		else if (res != 0xAA) {
			kprintf("[kernel][x86]: second ps2 port self-test failed\n");
			second_works = false;
		}

		ps2_data_read();

		if (second_works) {
			conf |= CONF_INT_PORT2;
		}
	}

	ps2_cmd_write(CMD_WRITE_CONF0);
	wait_for_completion();
	ps2_data_write(conf);
	wait_for_completion();

	if (first_works) {
		
	}
}