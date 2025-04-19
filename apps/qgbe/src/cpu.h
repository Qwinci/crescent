#pragma once
#include "types.h"

typedef enum {
	REG_A,
	REG_F,
	REG_B,
	REG_C,
	REG_D,
	REG_E,
	REG_H,
	REG_L,
	REG_AF,
	REG_BC,
	REG_DE,
	REG_HL,
	REG_SP,
	REG_MAX
} Reg;

#define F_Z (1 << 7)
#define F_N (1 << 6)
#define F_H (1 << 5)
#define F_C (1 << 4)

typedef struct Inst Inst;

typedef struct {
	struct Bus* bus;
	Inst* cur_inst;
	u16 sp;
	u16 pc;
	u16 fetched_data;
	u16 dest_addr;
	bool dest_is_mem;
	u8 ie;
	bool ime;
	u8 remaining_cycles;
	u8 regs[REG_MAX];
	u8 if_flag;
	bool halted;
} Cpu;

typedef u8 (*InstFn)(Cpu* self);

typedef enum : u8 {
	IRQ_VBLANK = 1 << 0,
	IRQ_LCD_STAT = 1 << 1,
	IRQ_TIMER = 1 << 2,
	IRQ_SERIAL = 1 << 3,
	IRQ_JOYPAD = 1 << 4
} Irq;

void cpu_cycle(Cpu* self);
u16 reg_read(Cpu* self, Reg reg);
void reg_write(Cpu* self, Reg reg, u16 value);
void cpu_request_irq(Cpu* self, Irq irq);
