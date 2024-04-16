#include "ps2.hpp"
#include "acpi/acpi.hpp"
#include "dev/clock.hpp"
#include "mem/portspace.hpp"
#include "mem/register.hpp"
#include "stdio.hpp"

namespace {
	PortSpace SPACE {0x60};
}

namespace regs {
	static constexpr BasicRegister<u8> DATA {0x0};
	static constexpr BitRegister<u8> STATUS {0x4};
	static constexpr BasicRegister<u8> CMD {0x4};
}

namespace status {
	static constexpr BitField<u8, bool> OUTPUT_FULL {0, 1};
	static constexpr BitField<u8, bool> INPUT_FULL {1, 1};
}

namespace controller_cmd {
	static constexpr u8 READ_CONFIG = 0x20;
	static constexpr u8 WRITE_CONFIG = 0x60;
	static constexpr u8 DISABLE_PORT2 = 0xA7;
	static constexpr u8 ENABLE_PORT2 = 0xA8;
	static constexpr u8 TEST_PORT2 = 0xA9;
	static constexpr u8 TEST_CONTROLLER = 0xAA;
	static constexpr u8 TEST_PORT1 = 0xAB;
	static constexpr u8 DISABLE_PORT1 = 0xAD;
	static constexpr u8 ENABLE_PORT1 = 0xAE;
	static constexpr u8 WRITE_TO_PORT2 = 0xD4;
}

namespace config {
	static constexpr u8 PORT1_IRQ = 1 << 0;
	static constexpr u8 PORT2_IRQ = 1 << 1;
	static constexpr u8 PORT2_CLOCk = 1 << 5;
	static constexpr u8 PORT1_TRANSLATION = 1 << 6;
}

static void send_cmd(u8 cmd) {
	while (SPACE.load(regs::STATUS) & status::INPUT_FULL);
	SPACE.store(regs::CMD, cmd);
}

bool ps2_has_data() {
	return SPACE.load(regs::STATUS) & status::OUTPUT_FULL;
}

u8 ps2_receive_data() {
	while (!ps2_has_data());
	return SPACE.load(regs::DATA);
}

static void write_data(u8 data) {
	while (SPACE.load(regs::STATUS) & status::INPUT_FULL);
	SPACE.store(regs::DATA, data);
}

static void write_data_port2(u8 data) {
	send_cmd(controller_cmd::WRITE_TO_PORT2);
	write_data(data);
}

void ps2_send_data(bool second, u8 data) {
	if (second) {
		while (true) {
			write_data_port2(data);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				return;
			}
		}
	}
	else {
		while (true) {
			write_data(data);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				return;
			}
		}
	}
}

void ps2_send_data2(bool second, u8 data1, u8 data2) {
	if (second) {
		while (true) {
			write_data_port2(data1);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				break;
			}
		}
		while (true) {
			write_data_port2(data2);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				break;
			}
		}
	}
	else {
		while (true) {
			write_data(data1);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				break;
			}
		}
		while (true) {
			write_data(data2);
			auto resp = ps2_receive_data();
			if (resp == 0xFA) {
				return;
			}
		}
	}
}

namespace cmd {
	static constexpr u8 IDENTIFY = 0xF2;
	static constexpr u8 ENABLE_SCANNING = 0xF4;
	static constexpr u8 DISABLE_SCANNING = 0xF5;
}

kstd::array<u8, 3> ps2_identify(bool second) {
	ps2_send_data(second, cmd::DISABLE_SCANNING);
	ps2_send_data(second, cmd::IDENTIFY);
	kstd::array<u8, 3> bytes {0xFF, 0xFF, 0xFF};
	u8 count = 0;
	with_timeout([&] {
		if (ps2_has_data()) {
			bytes[count++] = ps2_receive_data();
			return count == 3;
		}
		return false;
	}, US_IN_MS * 4);

	ps2_send_data(second, cmd::ENABLE_SCANNING);
	return bytes;
}

static constexpr u16 FADT_FLAG_8042 = 1 << 1;

void ps2_keyboard_init(bool second);
void ps2_mouse_init(bool second);

void x86_ps2_init() {
	if (!(acpi::GLOBAL_FADT->iapc_boot_arch & FADT_FLAG_8042)) {
		return;
	}

	send_cmd(controller_cmd::DISABLE_PORT1);
	send_cmd(controller_cmd::DISABLE_PORT2);

	while (SPACE.load(regs::STATUS) & status::OUTPUT_FULL) {
		SPACE.load(regs::DATA);
	}

	send_cmd(controller_cmd::READ_CONFIG);
	u8 config = ps2_receive_data();
	config &= ~config::PORT1_IRQ;
	config &= ~config::PORT2_IRQ;
	config &= ~config::PORT1_TRANSLATION;
	send_cmd(controller_cmd::WRITE_CONFIG);
	write_data(config);

	bool dual_channel = config & config::PORT2_CLOCk;

	send_cmd(controller_cmd::TEST_CONTROLLER);
	u8 test_resp = ps2_receive_data();
	if (test_resp != 0x55) {
		println("[kernel][x86]: error: ps2 controller replied to test with ", Fmt::Hex, test_resp, Fmt::Reset);
		return;
	}
	send_cmd(controller_cmd::WRITE_CONFIG);
	write_data(config);

	if (dual_channel) {
		send_cmd(controller_cmd::ENABLE_PORT2);
		send_cmd(controller_cmd::READ_CONFIG);
		config = ps2_receive_data();
		if (config & config::PORT2_CLOCk) {
			dual_channel = false;
		}
		else {
			println("[kernel][x86]: ps2 controller has two ports");

			send_cmd(controller_cmd::DISABLE_PORT2);
		}
	}

	send_cmd(controller_cmd::TEST_PORT1);
	bool port1_good = ps2_receive_data() == 0;
	bool port2_good;
	if (dual_channel) {
		send_cmd(controller_cmd::TEST_PORT2);
		port2_good = ps2_receive_data() == 0;
	}
	else {
		port2_good = false;
	}

	if (!port1_good && !port2_good) {
		return;
	}

	if (port1_good) {
		send_cmd(controller_cmd::ENABLE_PORT1);
	}
	if (port2_good) {
		send_cmd(controller_cmd::ENABLE_PORT2);
	}

	if (port1_good) {
		ps2_send_data(false, 0xFF);
		port1_good = ps2_receive_data() == 0xAA;
		if (SPACE.load(regs::STATUS) & status::OUTPUT_FULL) {
			SPACE.load(regs::DATA);
		}
	}
	if (port2_good) {
		ps2_send_data(true, 0xFF);
		port2_good = ps2_receive_data() == 0xAA;
		if (SPACE.load(regs::STATUS) & status::OUTPUT_FULL) {
			SPACE.load(regs::DATA);
		}
	}

	if (port1_good) {
		auto port1_bytes = ps2_identify(false);
		if (port1_bytes[0] == 0 || port1_bytes[0] == 0x3 || port1_bytes[0] == 0x4) {
			ps2_mouse_init(false);
		}
		else {
			ps2_keyboard_init(false);
		}
	}

	if (port2_good) {
		auto port2_bytes = ps2_identify(true);
		if (port2_bytes[0] == 0 || port2_bytes[0] == 0x3 || port2_bytes[0] == 0x4) {
			ps2_mouse_init(true);
		}
		else {
			ps2_keyboard_init(true);
		}
	}

	while (ps2_has_data()) {
		ps2_receive_data();
	}

	send_cmd(controller_cmd::READ_CONFIG);
	config = ps2_receive_data();
	if (port1_good) {
		config |= config::PORT1_IRQ;
	}
	if (port2_good) {
		config |= config::PORT2_IRQ;
	}
	send_cmd(controller_cmd::WRITE_CONFIG);
	write_data(config);
}
