#include "ps2.hpp"
#include "acpi/acpi.hpp"
#include "arch/irq.hpp"
#include "dev/clock.hpp"
#include "dev/dev.hpp"
#include "io_apic.hpp"
#include "manually_destroy.hpp"
#include "mem/portspace.hpp"
#include "mem/register.hpp"
#include "sched/mutex.hpp"
#include "stdio.hpp"
#include "x86/irq.hpp"

/*
 * This code has been ported from Linux i8042/libps2 driver.
 * Copyright (c) 1999-2004 Vojtech Pavlik
 * Copyright (c) 2004 Dmitry Torokhov
*/

namespace {
	PortSpace SPACE {0x60};
	IrqSpinlock<void> PS2_LOCK {};
	ManuallyDestroy<Mutex<void>> PS2_CMD_MUTEX {};
	u8 INITIAL_CONFIG = 0;
	u8 CURRENT_CONFIG = 0;
}

namespace regs {
	static constexpr BasicRegister<u8> DATA {0x0};
	static constexpr BitRegister<u8> STATUS {0x4};
	static constexpr BasicRegister<u8> CMD {0x4};
}

namespace status {
	static constexpr BitField<u8, bool> OUTPUT_FULL {0, 1};
	static constexpr BitField<u8, bool> INPUT_FULL {1, 1};
	static constexpr BitField<u8, bool> AUX_DATA {5, 1};
}

namespace controller_cmd {
	struct Cmd {
		u8 cmd;
		u8 params : 4;
		u8 ret : 4;
	};

	static constexpr Cmd READ_CONFIG {
		.cmd = 0x20,
		.params = 0,
		.ret = 1
	};

	static constexpr Cmd WRITE_CONFIG {
		.cmd = 0x60,
		.params = 1,
		.ret = 0
	};

	static constexpr Cmd DISABLE_PORT2 {
		.cmd = 0xA7,
		.params = 0,
		.ret = 0
	};

	static constexpr Cmd ENABLE_PORT2 {
		.cmd = 0xA8,
		.params = 0,
		.ret = 0
	};

	static constexpr Cmd TEST_PORT2 {
		.cmd = 0xA9,
		.params = 0,
		.ret = 1
	};

	static constexpr Cmd TEST_CONTROLLER {
		.cmd = 0xAA,
		.params = 0,
		.ret = 1
	};

	static constexpr Cmd PORT2_LOOP {
		.cmd = 0xD3,
		.params = 1,
		.ret = 1
	};

	static constexpr Cmd WRITE_TO_PORT2 {
		.cmd = 0xD4,
		.params = 1,
		.ret = 0
	};
}

namespace config {
	static constexpr u8 PORT1_IRQ = 1 << 0;
	static constexpr u8 PORT2_IRQ = 1 << 1;
	static constexpr u8 PORT1_CLOCK_DISABLE = 1 << 4;
	static constexpr u8 PORT2_CLOCK_DISABLE = 1 << 5;
	static constexpr u8 PORT1_TRANSLATION = 1 << 6;
}

static bool ps2_wait_for_space() {
	return with_timeout([]() {
		return !(SPACE.load(regs::STATUS) & status::INPUT_FULL);
	}, US_IN_MS * 500);
}

static bool ps2_wait_for_data() {
	return with_timeout([]() {
		return SPACE.load(regs::STATUS) & status::OUTPUT_FULL;
	}, US_IN_MS * 500);
}

bool ps2_send_cmd(controller_cmd::Cmd cmd, u8* params) {
	auto guard = PS2_LOCK.lock();

	if (!ps2_wait_for_space()) {
		return false;
	}

	SPACE.store(regs::CMD, cmd.cmd);
	for (int i = 0; i < cmd.params; ++i) {
		if (!ps2_wait_for_space()) {
			println("[kernel][ps2]: waiting for write timed out");
			return false;
		}
		SPACE.store(regs::DATA, params[i]);
	}

	for (int i = 0; i < cmd.ret; ++i) {
		if (!ps2_wait_for_data()) {
			println("[kernel][ps2]: waiting for read timed out");
			return false;
		}
		params[i] = SPACE.load(regs::DATA);
	}

	return true;
}

static bool ps2_flush_data() {
	auto guard = PS2_LOCK.lock();

	int i = 0;
	while (SPACE.load(regs::STATUS) & status::OUTPUT_FULL) {
		if (i == 16) {
			return false;
		}
		udelay(50);
		SPACE.load(regs::DATA);
		++i;
	}

	return true;
}

static bool ps2_enable_aux(bool enable) {
	auto cmd = enable ? controller_cmd::ENABLE_PORT2 : controller_cmd::DISABLE_PORT2;
	if (!ps2_send_cmd(cmd, nullptr)) {
		return false;
	}

	for (int i = 0; i < 100; ++i) {
		udelay(50);
		u8 config;
		if (!ps2_send_cmd(controller_cmd::READ_CONFIG, &config)) {
			return false;
		}
		if (!(config & config::PORT2_CLOCK_DISABLE) == enable) {
			return true;
		}
	}

	return false;
}

static bool ps2_enable_aux_in_config() {
	CURRENT_CONFIG &= ~config::PORT2_CLOCK_DISABLE;
	CURRENT_CONFIG |= config::PORT2_IRQ;
	if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
		CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
		CURRENT_CONFIG &= ~config::PORT2_IRQ;
		return false;
	}

	return true;
}

static bool ps2_enable_kb_in_config() {
	CURRENT_CONFIG &= ~config::PORT1_CLOCK_DISABLE;
	CURRENT_CONFIG |= config::PORT1_IRQ;
	if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
		CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
		CURRENT_CONFIG &= ~config::PORT2_IRQ;
		return false;
	}

	return true;
}

static bool ps2_detect_aux() {
	ps2_flush_data();

	u8 param = 0x5A;
	bool status = ps2_send_cmd(controller_cmd::PORT2_LOOP, &param);

	bool port2_loop_broken = false;

	if (!status || param != 0x5A) {
		auto test_status = ps2_send_cmd(controller_cmd::TEST_PORT2, &param);
		if (!test_status || (param && param != 0xFA && param != 0xFF)) {
			return false;
		}

		if (status) {
			port2_loop_broken = true;
		}
	}

	ps2_enable_aux(false);
	if (!ps2_enable_aux(true)) {
		return false;
	}

	if (port2_loop_broken) {
		CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
		CURRENT_CONFIG &= ~config::PORT2_IRQ;
		if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
			return false;
		}

		return true;
	}

	u32 irq = x86_alloc_irq(1, false);
	assert(irq);

	Event port2_irq_event {};

	IrqHandler port2_irq_handler {
		.fn = [&](IrqFrame*) {
			auto guard = PS2_LOCK.lock();
			auto status = SPACE.load(regs::STATUS);
			if (status & status::OUTPUT_FULL) {
				auto data = SPACE.load(regs::DATA);
				if ((status & status::AUX_DATA) && data == 0xA5) {
					port2_irq_event.signal_one();
				}
				return true;
			}

			return false;
		},
		.can_be_shared = true
	};

	register_irq_handler(irq, &port2_irq_handler);
	IO_APIC.register_isa_irq(12, irq);

	if (!ps2_enable_aux_in_config()) {
		CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
		CURRENT_CONFIG &= ~config::PORT2_IRQ;
		ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG);

		IO_APIC.deregister_isa_irq(12);
		deregister_irq_handler(irq, &port2_irq_handler);
		x86_dealloc_irq(irq, 1, true);
		return false;
	}

	auto cmd = controller_cmd::PORT2_LOOP;
	cmd.ret = 0;

	param = 0xA5;
	if (!ps2_send_cmd(cmd, &param)) {
		CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
		CURRENT_CONFIG &= ~config::PORT2_IRQ;
		ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG);

		IO_APIC.deregister_isa_irq(12);
		deregister_irq_handler(irq, &port2_irq_handler);
		x86_dealloc_irq(irq, 1, true);
		return false;
	}

	bool ret = true;

	if (!port2_irq_event.wait_with_timeout(US_IN_MS * 250)) {
		ps2_flush_data();
		ret = false;
	}

	CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
	CURRENT_CONFIG &= ~config::PORT2_IRQ;
	if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
		ret = false;
	}

	IO_APIC.deregister_isa_irq(12);
	deregister_irq_handler(irq, &port2_irq_handler);
	x86_dealloc_irq(irq, 1, true);

	return ret;
}

static constexpr u16 FADT_FLAG_8042 = 1 << 1;

static bool init_controller() {
	u8 config[2];
	for (int i = 0;; ++i) {
		if (i == 10) {
			println("[kernel][ps2]: failed to read stable config");
			return false;
		}

		if (i != 0) {
			udelay(50);
		}

		if (!ps2_send_cmd(controller_cmd::READ_CONFIG, &config[i % 2])) {
			println("[kernel][ps2]: failed to read config");
			return false;
		}

		if (i >= 2 && config[0] == config[1]) {
			break;
		}
	}

	INITIAL_CONFIG = config[0];
	CURRENT_CONFIG = config[0];

	CURRENT_CONFIG |= config::PORT1_CLOCK_DISABLE;
	CURRENT_CONFIG &= ~config::PORT1_IRQ;
	CURRENT_CONFIG &= ~config::PORT1_TRANSLATION;

	if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
		println("[kernel][ps2]: failed to write config");
		return false;
	}

	ps2_flush_data();
	return true;
}

struct Ps2PrimaryPort : Ps2Port {
	bool send(u8 byte) override {
		auto guard = PS2_LOCK.lock();
		if (!ps2_wait_for_space()) {
			return false;
		}
		SPACE.store(regs::DATA, byte);
		return true;
	}
};

struct Ps2SecondaryPort : Ps2Port {
	bool send(u8 byte) override {
		return ps2_send_cmd(controller_cmd::WRITE_TO_PORT2, &byte);
	}
};

static bool ps2_is_kb_id(u8 id) {
	return id == 0x2B || id == 0x47 || id == 0x5D ||
		id == 0x60 || id == 0xAB || id == 0xAC;
}

static u32 adjust_timeout(Ps2Port* port, u8 cmd, u32 timeout) {
	switch (cmd) {
		case ps2_cmd::RESET.cmd:
			if (timeout > 100) {
				timeout = 100;
			}
			break;
		case ps2_cmd::GET_ID.cmd:
			if (port->cmd_resp[1] == 0xAA) {
				auto old = port->lock.manual_lock();
				port->flags = Ps2PortFlags::None;
				port->lock.manual_unlock(old);
				timeout = 0;
			}

			if (!ps2_is_kb_id(port->cmd_resp[1])) {
				auto old = port->lock.manual_lock();
				port->flags = Ps2PortFlags::None;
				port->cmd_resp_count = 0;
				port->lock.manual_unlock(old);
				timeout = 0;
			}
			break;
		default:
			break;
	}

	return timeout;
}

bool Ps2Port::send_cmd(Ps2Cmd cmd, u8* param) {
	auto guard = PS2_CMD_MUTEX->lock();
	return send_cmd_unlocked(cmd, param);
}

bool Ps2Port::send_cmd_unlocked(Ps2Cmd cmd, u8* param) {
	auto old = lock.manual_lock();

	cmd_resp_count = cmd.ret;

	switch (cmd.cmd) {
		case ps2_cmd::GET_ID.cmd:
			flags |= Ps2PortFlags::CmdIsGetId;
			break;
		case ps2_cmd::SET_LEDS.cmd:
		case ps2_cmd::SET_LEDS_EX.cmd:
		case ps2_cmd::SET_REP.cmd:
			flags |= Ps2PortFlags::PassNonAck;
			break;
		default:
			flags = Ps2PortFlags::None;
			break;
	}

	if (cmd.ret) {
		flags |= Ps2PortFlags::WaitForCmd | Ps2PortFlags::WaitForCmdResp;
		for (int i = 0; i < cmd.ret; ++i) {
			cmd_resp[(cmd.ret - 1) - i] = param[i];
		}
	}

	u32 timeout_ms = cmd.cmd == ps2_cmd::RESET.cmd ? 1000 : 200;
	auto status = send_byte(cmd.cmd, timeout_ms, 2, old);
	if (!status) {
		flags = Ps2PortFlags::None;
		lock.manual_unlock(old);
		return false;
	}

	for (int i = 0; i < cmd.params; ++i) {
		status = send_byte(param[i], 200, 2, old);
		if (!status) {
			flags = Ps2PortFlags::None;
			lock.manual_unlock(old);
			return false;
		}
	}

	lock.manual_unlock(old);

	timeout_ms = cmd.cmd == ps2_cmd::RESET.cmd ? 4000 : 500;
	if (cmd.ret) {
		event.wait_with_timeout(timeout_ms * US_IN_MS);

		old = lock.manual_lock();
		if (cmd_resp_count && !(flags & Ps2PortFlags::WaitForCmdResp)) {
			lock.manual_unlock(old);
			timeout_ms = adjust_timeout(this, cmd.cmd, timeout_ms);
			event.wait_with_timeout(timeout_ms * US_IN_MS);
		}
		else {
			lock.manual_unlock(old);
		}
	}

	old = lock.manual_lock();

	for (int i = 0; i < cmd.ret; ++i) {
		param[i] = cmd_resp[(cmd.ret - 1) - i];
	}

	if (cmd_resp_count && (cmd.cmd != ps2_cmd::RESET.cmd || cmd_resp_count != 1)) {
		flags = Ps2PortFlags::None;
		lock.manual_unlock(old);
		return false;
	}

	flags = Ps2PortFlags::None;
	lock.manual_unlock(old);
	return true;
}

bool Ps2Port::send_byte(u8 byte, u32 timeout_ms, u32 max_retries, bool irq_state) {
	bool success = false;

	for (; max_retries; --max_retries) {
		ack = Ps2Ack::Nack;
		flags |= Ps2PortFlags::WaitForAck;

		lock.manual_unlock(irq_state);

		success = send(byte);
		if (success) {
			event.wait_with_timeout(timeout_ms * US_IN_MS);
		}

		irq_state = lock.manual_lock();

		if (ack != Ps2Ack::Nack) {
			break;
		}
	}

	flags &= ~Ps2PortFlags::WaitForAck;

	if (success && ack == Ps2Ack::Ack) {
		return true;
	}

	return false;
}

namespace {
	Ps2PrimaryPort PORT1 {};
	Ps2SecondaryPort PORT2 {};
	bool PORT2_PRESENT = false;
}

static void ps2_process_cmd_response(Ps2Port* port, u8 data) {
	if (port->cmd_resp_count) {
		port->cmd_resp[--port->cmd_resp_count] = data;
	}

	if (port->flags & Ps2PortFlags::WaitForCmdResp) {
		port->flags &= ~Ps2PortFlags::WaitForCmdResp;
		if (port->cmd_resp_count) {
			port->event.signal_one();
		}
	}

	if (!port->cmd_resp_count) {
		port->flags &= ~Ps2PortFlags::WaitForCmd;
		port->event.signal_one();
	}
}

static void ps2_process_ack(Ps2Port* port, u8 data) {
	switch (data) {
		case 0xFA:
			port->ack = Ps2Ack::Ack;
			break;
		case 0xFE:
			port->ack = Ps2Ack::Nack;
			break;
		case 0xFC:
			if (port->flags & Ps2PortFlags::LastWasNack) {
				port->flags &= ~Ps2PortFlags::LastWasNack;
				port->ack = Ps2Ack::Error;
				break;
			}
			[[fallthrough]];
		case 0:
		case 3:
		case 4:
			if (port->flags & Ps2PortFlags::CmdIsGetId) {
				port->ack = Ps2Ack::Ack;
				break;
			}
			[[fallthrough]];
		default:
			if (port->device && port->flags & Ps2PortFlags::PassNonAck) {
				port->device->on_receive(data);
			}
			port->flags &= ~Ps2PortFlags::CmdIsGetId;
			port->flags &= ~Ps2PortFlags::PassNonAck;
			return;
	}

	if (port->ack == Ps2Ack::Ack) {
		port->flags &= ~Ps2PortFlags::LastWasNack;
	}

	port->flags &= ~Ps2PortFlags::WaitForAck;

	if (port->ack == Ps2Ack::Ack && data != 0xFA) {
		ps2_process_cmd_response(port, data);
	}
	else {
		port->event.signal_one();
	}
}

bool ps2_irq(IrqFrame*) {
	auto guard = PS2_LOCK.lock();

	auto status = SPACE.load(regs::STATUS);
	if (!(status & status::OUTPUT_FULL)) {
		return false;
	}

	Ps2Port* port = nullptr;

	auto data = SPACE.load(regs::DATA);
	if (PORT2_PRESENT && (status & status::AUX_DATA)) {
		port = &PORT2;
	}
	else if (!(status & status::AUX_DATA)) {
		port = &PORT1;
	}

	if (port) {
		auto port_guard = port->lock.lock();

		if (port->flags & Ps2PortFlags::WaitForAck) {
			ps2_process_ack(port, data);
		}
		else if (port->flags & Ps2PortFlags::WaitForCmd) {
			ps2_process_cmd_response(port, data);
		}
		else {
			if (port->device) {
				port->device->on_receive(data);
			}
		}
	}

	return true;
}

namespace {
	ManuallyDestroy<IrqHandler> PS2_PORT1_IRQ {{
		.fn = ps2_irq,
		.can_be_shared = true
	}};
	ManuallyDestroy<IrqHandler> PS2_PORT2_IRQ {{
		.fn = ps2_irq,
		.can_be_shared = true
	}};

	u32 PORT1_IRQ;
	u32 PORT2_IRQ = 0xFFFF;

	struct Ps2Controller : Device {
		Ps2Controller() {
			dev_add(this);
		}

		int suspend() override {
			ps2_flush_data();

			if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &INITIAL_CONFIG)) {
				println("[kernel][ps2]: failed to write config before suspend");
			}

			return 0;
		}

		int resume() override {
			if (!ps2_flush_data()) {
				println("[kernel][ps2]: failed to resume controller");
				return 0;
			}

			CURRENT_CONFIG = INITIAL_CONFIG;
			CURRENT_CONFIG &= ~config::PORT1_TRANSLATION;
			CURRENT_CONFIG |= config::PORT1_CLOCK_DISABLE;
			CURRENT_CONFIG |= config::PORT2_CLOCK_DISABLE;
			CURRENT_CONFIG &= ~config::PORT1_IRQ;
			CURRENT_CONFIG &= ~config::PORT2_IRQ;
			if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
				println("[kernel][ps2]: failed to write config after resume, retrying");
				mdelay(50);
				if (!ps2_send_cmd(controller_cmd::WRITE_CONFIG, &CURRENT_CONFIG)) {
					println("[kernel][ps2]: failed to write config after resume");
					return 0;
				}
			}

			IO_APIC.register_isa_irq(1, PORT1_IRQ);

			if (PORT2_PRESENT) {
				IO_APIC.register_isa_irq(12, PORT2_IRQ);
				ps2_enable_aux_in_config();
			}

			ps2_enable_kb_in_config();

			ps2_irq(nullptr);

			if (PORT1.device) {
				PORT1.device->resume();
			}
			if (PORT2.device) {
				PORT2.device->resume();
			}

			return 0;
		}
	};

	ManuallyInit<Ps2Controller> CONTROLLER;
}

bool ps2_keyboard_init(Ps2Port* port);
bool ps2_mouse_init(Ps2Port* port);

void x86_ps2_init() {
	if (!init_controller()) {
		return;
	}

	if (ps2_detect_aux()) {
		PORT2_IRQ = x86_alloc_irq(1, false);
		assert(PORT2_IRQ);

		register_irq_handler(PORT2_IRQ, &*PS2_PORT2_IRQ);
		IO_APIC.register_isa_irq(12, PORT2_IRQ);

		if (!ps2_enable_aux_in_config()) {
			IO_APIC.deregister_isa_irq(12);
			deregister_irq_handler(PORT2_IRQ, &*PS2_PORT2_IRQ);
			x86_dealloc_irq(PORT2_IRQ, 1, true);
			PORT2_IRQ = 0xFFFF;
		}
		else {
			PORT2_PRESENT = true;
		}
	}

	if (!ps2_enable_kb_in_config()) {
		if (PORT2_IRQ != 0xFFFF) {
			IO_APIC.deregister_isa_irq(12);
			deregister_irq_handler(PORT2_IRQ, &*PS2_PORT2_IRQ);
			x86_dealloc_irq(PORT2_IRQ, 1, true);
		}
		return;
	}

	PORT1_IRQ = x86_alloc_irq(1, true);
	assert(PORT1_IRQ);

	register_irq_handler(PORT1_IRQ, &*PS2_PORT1_IRQ);
	IO_APIC.register_isa_irq(1, PORT1_IRQ);

	CONTROLLER.initialize();

	if (PORT2_PRESENT) {
		if (!ps2_mouse_init(&PORT2)) {
			ps2_keyboard_init(&PORT2);
		}
	}

	if (!ps2_keyboard_init(&PORT1)) {
		ps2_mouse_init(&PORT1);
	}
}
