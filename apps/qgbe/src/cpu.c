#include "cpu.h"
#include "bus.h"
#include "dis.h"
#include "inst.h"
#include <stdio.h>
#include <stdlib.h>

extern InstFn INST_FNS[T_MAX];

u16 reg_read(Cpu* self, Reg reg) {
	if (reg < REG_AF) {
		return self->regs[reg];
	}
	else if (reg == REG_AF) {
		return self->regs[REG_F] | self->regs[REG_A] << 8;
	}
	else if (reg == REG_BC) {
		return self->regs[REG_C] | self->regs[REG_B] << 8;
	}
	else if (reg == REG_DE) {
		return self->regs[REG_E] | self->regs[REG_D] << 8;
	}
	else if (reg == REG_HL) {
		return self->regs[REG_L] | self->regs[REG_H] << 8;
	}
	else {
		return self->sp;
	}
}

void reg_write(Cpu* self, Reg reg, u16 value) {
	if (reg < REG_AF) {
		if (reg == REG_F) {
			self->regs[REG_F] = value & 0xF0;
		}
		else {
			self->regs[reg] = value;
		}
	}
	else if (reg == REG_AF) {
		self->regs[REG_F] = value & 0xF0;
		self->regs[REG_A] = value >> 8;
	}
	else if (reg == REG_BC) {
		self->regs[REG_C] = value;
		self->regs[REG_B] = value >> 8;
	}
	else if (reg == REG_DE) {
		self->regs[REG_E] = value;
		self->regs[REG_D] = value >> 8;
	}
	else if (reg == REG_HL) {
		self->regs[REG_L] = value;
		self->regs[REG_H] = value >> 8;
	}
	else {
		self->sp = value;
	}
}

static u8 cpu_fetch(Cpu* self) {
	self->dest_is_mem = false;

	switch (self->cur_inst->mode) {
		case M_IMP:
			return 0;
		case M_U8:
			self->fetched_data = bus_read(self->bus, self->pc++);
			return 1;
		case M_U16:
			self->fetched_data = bus_read(self->bus, self->pc++);
			self->fetched_data |= bus_read(self->bus, self->pc++) << 8;
			return 2;
		case M_R:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			return 0;
		case M_MR_R:
			self->dest_addr = reg_read(self, self->cur_inst->rd);
			self->dest_is_mem = true;
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			return 0;
		case M_MR:
			self->fetched_data = bus_read(self->bus, reg_read(self, self->cur_inst->rs));
			return 1;
		case M_MR_MR:
		{
			u16 addr = reg_read(self, self->cur_inst->rs);
			self->fetched_data = bus_read(self->bus, addr);
			self->dest_addr = addr;
			self->dest_is_mem = true;
			return 1;
		}
		case M_MR_U8:
			self->fetched_data = bus_read(self->bus, self->pc++);
			self->dest_addr = reg_read(self, self->cur_inst->rd);
			self->dest_is_mem = true;
			return 1;
		case M_MRI_R:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			self->dest_addr = reg_read(self, self->cur_inst->rd);
			self->dest_is_mem = true;
			reg_write(self, self->cur_inst->rd, self->dest_addr + 1);
			return 0;
		case M_MRD_R:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			self->dest_addr = reg_read(self, self->cur_inst->rd);
			self->dest_is_mem = true;
			reg_write(self, self->cur_inst->rd, self->dest_addr - 1);
			return 0;
		case M_MRI:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			reg_write(self, self->cur_inst->rs, self->fetched_data + 1);
			self->fetched_data = bus_read(self->bus, self->fetched_data);
			return 1;
		case M_MRD:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			reg_write(self, self->cur_inst->rs, self->fetched_data - 1);
			self->fetched_data = bus_read(self->bus, self->fetched_data);
			return 1;
		case M_R_M_U8:
			// not really dest_addr but just used as tmp
			self->dest_addr = 0xFF00 + bus_read(self->bus, self->pc++);
			self->fetched_data = bus_read(self->bus, self->dest_addr);
			return 2;
		case M_R_M_U16:
			// not really dest_addr but just used as tmp
			self->dest_addr = bus_read(self->bus, self->pc++);
			self->dest_addr |= bus_read(self->bus, self->pc++) << 8;
			self->fetched_data = bus_read(self->bus, self->dest_addr);
			return 3;
		case M_M_U8:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			self->dest_addr = 0xFF00 + bus_read(self->bus, self->pc++);
			self->dest_is_mem = true;
			return 1;
		case M_M_U16_R:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			self->dest_addr = bus_read(self->bus, self->pc++);
			self->dest_addr |= bus_read(self->bus, self->pc++) << 8;
			self->dest_is_mem = true;
			return 2;
		case M_SP_I8:
		{
			i8 value = (i8) bus_read(self->bus, self->pc++);
			u32 sum = self->sp + value;
			u32 sum_nc = self->sp ^ value;
			u32 c_diff = sum ^ sum_nc;
			bool c = c_diff & 1 << 8;
			bool hc = c_diff & 1 << 4;
			self->fetched_data = sum;
			self->regs[REG_F] = (hc ? F_H : 0) | (c ? F_C : 0);
			return 2;
		}
		case M_MR8_R:
			self->fetched_data = reg_read(self, self->cur_inst->rs);
			self->dest_addr = 0xFF00 + reg_read(self, self->cur_inst->rd);
			self->dest_is_mem = true;
			return 0;
		case M_MR8:
			// not really dest_addr but just used as tmp
			self->dest_addr = 0xFF00 + reg_read(self, self->cur_inst->rs);
			self->fetched_data = bus_read(self->bus, self->dest_addr);
			return 1;
	}

	return 0;
}

void cpu_request_irq(Cpu* self, Irq irq) {
	self->if_flag |= (u8) irq;
}

static u16 IRQ_LOCATIONS[5] = {
	[0] = 0x40,
	[1] = 0x48,
	[2] = 0x50,
	[3] = 0x58,
	[4] = 0x60
};

bool cpu_process_irqs(Cpu* self) {
	if (self->ime && self->if_flag & self->ie) {
		for (u8 i = 0; i < 5; ++i) {
			if ((self->ie & 1 << i) && (self->if_flag & 1 << i)) {
				self->if_flag &= ~(1 << i);
				self->ime = false;

				bus_write(self->bus, --self->sp, self->pc >> 8);
				bus_write(self->bus, --self->sp, self->pc);

				self->halted = false;
				self->pc = IRQ_LOCATIONS[i];
				self->remaining_cycles = 5;
				return true;
			}
		}
	}
	else if (self->if_flag && self->halted) {
		self->halted = false;
		return true;
	}

	return false;
}

static usize INST_COUNT = 0;

void cpu_cycle(Cpu* self) {
	if (self->remaining_cycles) {
		self->remaining_cycles -= 1;
		return;
	}

	if (cpu_process_irqs(self)) {
		timer_cycle(&self->bus->timer);
		return;
	}
	if (self->halted) {
		timer_cycle(&self->bus->timer);
		return;
	}

	/*printf("%zu  TIMA: %02X TMA: %02X DIV: %02X TAC: %02X IME: %02X IE: %02X IF: %02X "
		   "A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
		   INST_COUNT, self->bus->timer.tima, self->bus->timer.tma, self->bus->timer.div >> 8, self->bus->timer.tac,
		   self->ime ? 1 : 0, self->ie, self->if_flag,
		   self->regs[REG_A], self->regs[REG_F], self->regs[REG_B], self->regs[REG_C],
		   self->regs[REG_D], self->regs[REG_E], self->regs[REG_H], self->regs[REG_L],
		   self->sp, self->pc, bus_read(self->bus, self->pc), bus_read(self->bus, self->pc + 1),
		   bus_read(self->bus, self->pc + 2), bus_read(self->bus, self->pc + 3));

	INST_COUNT += 1;*/

	/*if (self->pc >= 0x100) {
		printf("A: %02X F: %02X B: %02X C: %02X D: %02X E: %02X H: %02X L: %02X SP: %04X PC: 00:%04X (%02X %02X %02X %02X)\n",
			   self->regs[REG_A], self->regs[REG_F], self->regs[REG_B], self->regs[REG_C],
			   self->regs[REG_D], self->regs[REG_E], self->regs[REG_H], self->regs[REG_L],
			   self->sp, self->pc, bus_read(self->bus, self->pc), bus_read(self->bus, self->pc + 1),
			   bus_read(self->bus, self->pc + 2), bus_read(self->bus, self->pc + 3));
	}*/

	u16 start_pc = self->pc;
	u8 op = bus_read(self->bus, self->pc++);
	Inst* inst = &INSTRUCTIONS[op];

	self->cur_inst = inst;
	self->remaining_cycles += cpu_fetch(self);

	//print_inst(self, inst, start_pc);

	// A: 01 F: B0 B: 00 C: 13 D: 00 E: D8 H: 01 L: 4D SP: FFFE PC: 00:0100 (00 C3 13 02)

	if (!INST_FNS[inst->type]) {
		fputs("unimplemented instruction\n", stderr);
		exit(1);
	}
	self->remaining_cycles += INST_FNS[inst->type](self);

	/*const u8 TIMINGS[] = {
		1,3,2,2,1,1,2,1,5,2,2,2,1,1,2,1,
		0,3,2,2,1,1,2,1,3,2,2,2,1,1,2,1,
		2,3,2,2,1,1,2,1,2,2,2,2,1,1,2,1,
		2,3,2,2,3,3,3,1,2,2,2,2,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		2,2,2,2,2,2,0,2,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		1,1,1,1,1,1,2,1,1,1,1,1,1,1,2,1,
		2,3,3,4,3,4,2,4,2,4,3,0,3,6,2,4,
		2,3,3,0,3,4,2,4,2,4,3,0,3,0,2,4,
		3,3,2,0,0,4,2,4,4,1,4,0,0,0,2,4,
		3,3,2,1,0,4,2,4,3,2,4,1,0,0,2,4
	};

	const u8 CB_TIMINGS[] = {
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
		2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
		2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
		2,2,2,2,2,2,3,2,2,2,2,2,2,2,3,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2,
		2,2,2,2,2,2,4,2,2,2,2,2,2,2,4,2
	};

	if (inst->type == T_CB) {
		if (self->remaining_cycles != CB_TIMINGS[self->fetched_data]) {
			fprintf(stderr, "invalid timing for:\n");
			print_inst(self, inst, start_pc);
			fprintf(stderr, "expected: %u got: %u\n", CB_TIMINGS[self->fetched_data], self->remaining_cycles);
		}
	}
	else if (self->remaining_cycles != TIMINGS[op] && inst->type != T_JR && inst->type != T_RET) {
		fprintf(stderr, "invalid timing for:\n");
		print_inst(self, inst, start_pc);
		fprintf(stderr, "expected: %u got: %u\n", TIMINGS[op], self->remaining_cycles);
	}*/

	for (u8 i = 0; i < self->remaining_cycles; ++i) {
		timer_cycle(&self->bus->timer);
	}
}
