#pragma once
#include "types.h"

typedef enum : u32 {
	LAPIC_REG_ID = 0x20,
	LAPIC_REG_VERSION = 0x30,
	LAPIC_REG_TASK_PRIORITY = 0x80,
	LAPIC_REG_ARBITRATION_PRIORITY = 0x90,
	LAPIC_REG_PROCESSOR_PRIORITY = 0xA0,
	LAPIC_REG_EOI = 0xB0,
	LAPIC_REG_REMOTE_READ = 0xC0,
	LAPIC_REG_LOGICAL_DEST = 0xD0,
	LAPIC_REG_DEST_FORMAT = 0xE0,
	LAPIC_REG_SPURIOUS_INT = 0xF0,
	LAPIC_REG_IN_SERVICE_BASE = 0x100,
	LAPIC_REG_TRIGGER_MODE_BASE = 0x180,
	LAPIC_REG_INT_REQUEST_BASE = 0x200,
	LAPIC_REG_ERROR_STATUS = 0x280,
	LAPIC_REG_LVT_CORRECTED_MCE_INT = 0x2F0,
	LAPIC_REG_INT_CMD_BASE = 0x300,
	LAPIC_REG_LVT_TIMER = 0x320,
	LAPIC_REG_LVT_THERMAL_SENSOR = 0x330,
	LAPIC_REG_LVT_PERFMON_COUNTERS = 0x340,
	LAPIC_REG_LVT_LINT0 = 0x350,
	LAPIC_REG_LVT_LINT1 = 0x360,
	LAPIC_REG_LVT_ERROR = 0x370,
	LAPIC_REG_INIT_COUNT = 0x380,
	LAPIC_REG_CURR_COUNT = 0x390,
	LAPIC_REG_DIV_CONF = 0x3E0
} LapicReg;

typedef enum {
	LAPIC_MSG_HALT,
	LAPIC_MSG_PANIC,
	LAPIC_MSG_INVALIDATE
} LapicMsg;

void lapic_init();
void lapic_first_init();
u32 lapic_read(LapicReg reg);
void lapic_write(LapicReg reg, u32 value);
void lapic_eoi();
void lapic_ipi(u8 id, LapicMsg msg);
void lapic_ipi_all(LapicMsg msg);
void lapic_invalidate_mapping(u8 id, void* map);
