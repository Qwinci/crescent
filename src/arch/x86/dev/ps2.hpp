#pragma once
#include "array.hpp"
#include "dev/event.hpp"
#include "types.hpp"
#include "utils/flags_enum.hpp"

struct Ps2Cmd {
	u8 cmd;
	u8 params : 4;
	u8 ret : 4;
};

namespace ps2_cmd {
	inline constexpr Ps2Cmd SET_LEDS_EX {
		.cmd = 0xEB,
		.params = 2,
		.ret = 0
	};

	inline constexpr Ps2Cmd SET_LEDS {
		.cmd = 0xED,
		.params = 1,
		.ret = 0
	};

	inline constexpr Ps2Cmd GET_ID {
		.cmd = 0xF2,
		.params = 0,
		.ret = 2
	};

	static constexpr Ps2Cmd ENABLE_SCANNING {
		.cmd = 0xF4,
		.params = 0,
		.ret = 0
	};

	static constexpr Ps2Cmd DISABLE_SCANNING {
		.cmd = 0xF5,
		.params = 0,
		.ret = 0
	};

	static constexpr Ps2Cmd SET_DEFAULTS {
		.cmd = 0xF6,
		.params = 0,
		.ret = 0
	};

	inline constexpr Ps2Cmd SET_REP {
		.cmd = 0xF3,
		.params = 1,
		.ret = 0
	};

	inline constexpr Ps2Cmd RESET {
		.cmd = 0xFF,
		.params = 0,
		.ret = 2
	};
}

enum class Ps2PortFlags {
	None = 0,
	WaitForAck = 1 << 0,
	WaitForCmd = 1 << 1,
	WaitForCmdResp = 1 << 2,
	PassNonAck = 1 << 3,
	CmdIsGetId = 1 << 4,
	LastWasNack = 1 << 5
};
FLAGS_ENUM(Ps2PortFlags);

struct IrqFrame;

enum class Ps2Ack {
	Ack,
	Nack,
	Error
};

struct Ps2Device {
	virtual ~Ps2Device() = default;
	virtual void on_receive(u8 byte) = 0;
};

struct Ps2Port {
	virtual bool send(u8 byte) = 0;

	bool send_cmd(Ps2Cmd cmd, u8* param);
	bool send_cmd_unlocked(Ps2Cmd cmd, u8* param);

	Ps2Device* device {};
	Event event {};
	IrqSpinlock<void> lock {};
	Ps2PortFlags flags {};
	u8 cmd_resp[8] {};
	u8 cmd_resp_count {};
	Ps2Ack ack {};

private:
	enum class Status {
		Success,
		TimeOut,
		Nack
	};

	bool send_byte(u8 byte, u32 timeout_ms, u32 max_retries);
};

void x86_ps2_init();
