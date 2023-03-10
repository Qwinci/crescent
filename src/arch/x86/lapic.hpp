#pragma once
#include "types.hpp"

struct Lapic {
	static constexpr u32 INT_MASKED = 0x10000;

	enum class Reg : u32 {
		Id = 0x20,
		Version = 0x30,
		TaskPriority = 0x80,
		ArbitrationPriority = 0x90,
		ProcessorPriority = 0xA0,
		Eoi = 0xB0,
		RemoteRead = 0xC0,
		LogicalDest = 0xD0,
		DestFormat = 0xE0,
		SpuriousInt = 0xF0,
		InServiceBase = 0x100,
		TriggerModeBase = 0x180,
		IntRequestBase = 0x200,
		ErrorStatus = 0x280,
		LvtCorrectedMceInt = 0x2F0,
		IntCmdBase = 0x300,
		LvtTimer = 0x320,
		LvtThermalSensor = 0x330,
		LvtPerfMonCounters = 0x340,
		LvtLint0 = 0x350,
		LvtLint1 = 0x360,
		LvtError = 0x370,
		InitCount = 0x380,
		CurrCount = 0x390,
		DivConf = 0x3E0
	};

	static void init();

	static void write(Reg reg, u32 value);
	static u32 read(Reg reg);

	static void calibrate_timer();

	static void start_periodic(u64 frequency);
	static void start_oneshot(u64 frequency, u8 vec);
	static void eoi();

	enum class Msg {
		None,
		Halt,
		LoadBalance
	};

	static void send_ipi_all(Msg msg);
	static void send_ipi(u8 id, Msg msg);
private:
	static Msg current_msg;
	static usize base;

	friend void parse_madt(const void* madt_ptr);
	friend void ipi_handler(struct InterruptCtx* ctx, void*);
};