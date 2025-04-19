#include "bus.h"
#include "cpu.h"
#include "inst.h"

static u8 inst_ld(Cpu* self) {
	if (self->dest_is_mem) {
		bus_write(self->bus, self->dest_addr, self->fetched_data);
		return 2;
	}
	else {
		reg_write(self, self->cur_inst->rd, self->fetched_data);
		return 1;
	}
}

static u8 inst_ld16(Cpu* self) {
	bus_write(self->bus, self->dest_addr, self->fetched_data);
	bus_write(self->bus, self->dest_addr + 1, self->fetched_data >> 8);
	return 1;
}

static u8 inst_xor(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;
	u8 res = value ^ value2;

	self->regs[REG_F] = res == 0 ? F_Z : 0;
	self->regs[REG_A] = res;
	return 1;
}

static Reg CB_REGS[] = {
	[0] = REG_B,
	[1] = REG_C,
	[2] = REG_D,
	[3] = REG_E,
	[4] = REG_H,
	[5] = REG_L,
	[7] = REG_A
};

static u8 cb_reg_read(Cpu* self, u8 i, u8* cycles) {
	if (i == 6) {
		*cycles = 1;
		return bus_read(self->bus, reg_read(self, REG_HL));
	}
	else {
		*cycles = 0;
		return self->regs[CB_REGS[i]];
	}
}

static u8 cb_reg_write(Cpu* self, u8 i, u16 value) {
	if (i == 6) {
		bus_write(self->bus, reg_read(self, REG_HL), value);
		return 1;
	}
	else {
		self->regs[CB_REGS[i]] = value;
		return 0;
	}
}

static u8 inst_cb(Cpu* self) {
	u8 op = self->fetched_data;
	u8 reg_idx = op & 7;
	u8 cycles;
	u8 value = cb_reg_read(self, reg_idx, &cycles);

	if (op <= 0xF) {
		// RLC
		if (op <= 7) {
			value = (value << 1) | (value >> 7);
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (value & 1) {
				self->regs[REG_F] |= F_C;
			}
		}
		// RRC
		else {
			value = (value >> 1) | (value << 7);
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (value & 1 << 7) {
				self->regs[REG_F] |= F_C;
			}
		}
	}
	else if (op <= 0x1F) {
		// RL
		if (op <= 0x17) {
			bool c = value & 1 << 7;
			value = (value << 1) | ((self->regs[REG_F] & F_C) > 0);
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (c) {
				self->regs[REG_F] |= F_C;
			}
		}
		// RR
		else {
			bool c = value & 1;
			value = (value >> 1) | ((self->regs[REG_F] & F_C) > 0) << 7;
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (c) {
				self->regs[REG_F] |= F_C;
			}
		}
	}
	else if (op <= 0x2F) {
		// SLA
		if (op <= 0x27) {
			bool c = value & 1 << 7;
			value = value << 1;
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (c) {
				self->regs[REG_F] |= F_C;
			}
		}
		// SRA
		else {
			bool c = value & 1;
			value = value >> 1 | (value & 1 << 7);
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (c) {
				self->regs[REG_F] |= F_C;
			}
		}
	}
	else if (op <= 0x3F) {
		// SWAP
		if (op <= 0x37) {
			value = (value >> 4) | (value << 4);
			self->regs[REG_F] = value == 0 ? F_Z : 0;
		}
		// SRL
		else {
			bool c = value & 1;
			value = value >> 1;
			self->regs[REG_F] = 0;
			if (!value) {
				self->regs[REG_F] |= F_Z;
			}
			if (c) {
				self->regs[REG_F] |= F_C;
			}
		}
	}
	// BIT
	else if (op <= 0x7F) {
		u8 bit = (op - 0x40) >> 3;
		self->regs[REG_F] = (self->regs[REG_F] & F_C) | F_H | (!(value & 1 << bit) ? F_Z : 0);
		return 1 + cycles;
	}
	// RES
	else if (op <= 0xBF) {
		u8 bit = (op - 0x80) >> 3;
		value &= ~(1 << bit);
	}
	// SET
	else {
		u8 bit = (op - 0xC0) >> 3;
		value |= (1 << bit);
	}

	cycles += cb_reg_write(self, reg_idx, value);

	return 1 + cycles;
}

static bool check_cond(Cpu* self) {
	Inst* inst = self->cur_inst;
	u8 flags = self->regs[REG_F];
	if ((inst->cond == C_NONE) ||
		(inst->cond == C_NZ && !(flags & F_Z)) ||
		(inst->cond == C_NC && !(flags & F_C)) ||
		(inst->cond == C_C && (flags & F_C)) ||
		(inst->cond == C_Z && (flags & F_Z))) {
		return true;
	}
	else {
		return false;
	}
}

static u8 inst_jr(Cpu* self) {
	if (!check_cond(self)) {
		return 1;
	}

	self->pc += (i8) self->fetched_data;

	return 2;
}

static u8 inst_inc(Cpu* self) {
	if (!self->dest_is_mem && self->cur_inst->rd >= REG_AF) {
		reg_write(self, self->cur_inst->rd, self->fetched_data + 1);
		return 2;
	}

	bool hc = ((self->fetched_data & 0xF) + 1) & 1 << 4;
	u8 res = (u8) self->fetched_data + 1;

	self->regs[REG_F] = (self->regs[REG_F] & F_C) | (hc ? F_H : 0) |
						(res == 0 ? F_Z : 0);
	if (self->dest_is_mem) {
		bus_write(self->bus, self->dest_addr, res);
		return 2;
	}
	else {
		self->regs[self->cur_inst->rd] = res;
		return 1;
	}
}

static u8 inst_call(Cpu* self) {
	if (!check_cond(self)) {
		return 1;
	}

	bus_write(self->bus, --self->sp, self->pc >> 8);
	bus_write(self->bus, --self->sp, self->pc);
	self->pc = self->fetched_data;

	return 4;
}

static u8 inst_push(Cpu* self) {
	bus_write(self->bus, --self->sp, self->fetched_data >> 8);
	bus_write(self->bus, --self->sp, self->fetched_data);

	return 4;
}

static u8 inst_rla(Cpu* self) {
	u8 c = (self->regs[REG_F] & F_C) ? 1 : 0;
	u8 value = self->regs[REG_A];
	self->regs[REG_F] = (value & 1 << 7) ? F_C : 0;
	self->regs[REG_A] = value << 1 | c;
	return 1;
}

static u8 inst_pop(Cpu* self) {
	u16 value = bus_read(self->bus, self->sp++);
	value |= bus_read(self->bus, self->sp++) << 8;

	reg_write(self, self->cur_inst->rd, value);

	return 3;
}

static u8 inst_dec(Cpu* self) {
	if (!self->dest_is_mem && self->cur_inst->rd >= REG_AF) {
		reg_write(self, self->cur_inst->rd, self->fetched_data - 1);
		return 2;
	}

	bool hc = ((self->fetched_data & 0xF) - 1) & 1 << 4;
	u8 res = (u8) self->fetched_data - 1;

	self->regs[REG_F] = (self->regs[REG_F] & F_C) | (hc ? F_H : 0) |
						(res == 0 ? F_Z : 0) | F_N;
	if (self->dest_is_mem) {
		bus_write(self->bus, self->dest_addr, res);
		return 2;
	}
	else {
		self->regs[self->cur_inst->rd] = res;
		return 1;
	}
}

static u8 inst_ret(Cpu* self) {
	if (self->cur_inst->cond == C_NONE) {
		self->pc = bus_read(self->bus, self->sp++);
		self->pc |= bus_read(self->bus, self->sp++) << 8;
		return 4;
	}
	else if (!check_cond(self)) {
		return 2;
	}

	self->pc = bus_read(self->bus, self->sp++);
	self->pc |= bus_read(self->bus, self->sp++) << 8;

	return 5;
}

static char flag_buf[5] = {};

const char* get_flags(u8 flags) {
	flag_buf[0] = flags & F_C ? 'C' : ' ';
	flag_buf[1] = flags & F_H ? 'H' : ' ';
	flag_buf[2] = flags & F_Z ? 'Z' : ' ';
	flag_buf[3] = flags & F_N ? 'N' : ' ';
	return flag_buf;
}

static u8 inst_cp(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;

	bool hc = ((value & 0xF) - (value2 & 0xF)) & 1 << 4;
	u8 res = value - value2;
	bool c = value2 > value;

	self->regs[REG_F] = (res == 0 ? F_Z : 0) | (hc ? F_H : 0) | (c ? F_C : 0) | F_N;

	return 1;
}

static u8 inst_sub(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;

	bool hc = ((value & 0xF) - (value2 & 0xF)) & 1 << 4;
	u8 res = value - value2;
	bool c = value2 > value;

	self->regs[REG_A] = res;
	self->regs[REG_F] = (res == 0 ? F_Z : 0) | (hc ? F_H : 0) | (c ? F_C : 0) | F_N;

	return 1;
}

static u8 inst_add(Cpu* self) {
	if (self->cur_inst->rd == REG_SP) {
		u16 value = self->sp;
		i8 value2 = (i8) self->fetched_data;
		u32 sum = value + value2;
		u32 sum_nc = value ^ value2;
		u32 c_diff = sum ^ sum_nc;
		bool c = c_diff & 1 << 8;
		bool hc = c_diff & 1 << 4;
		self->regs[REG_F] = (hc ? F_H : 0) | (c ? F_C : 0);
		self->sp = sum & 0xFFFF;
		return 3;
	}
	else if (self->cur_inst->rd >= REG_AF) {
		u16 value = reg_read(self, self->cur_inst->rd);
		u16 value2 = self->fetched_data;
		bool hc = ((value & 0xFFF) + (value2 & 0xFFF)) & 1 << 12;
		u32 res = value + value2;
		self->regs[REG_F] = (self->regs[REG_F] & F_Z) | (hc ? F_H : 0) | (res > 0xFFFF ? F_C : 0);
		reg_write(self, self->cur_inst->rd, res & 0xFFFF);
		return 2;
	}

	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;

	u16 sum = value + value2;
	u16 no_carry_sum = value ^ value2;
	u16 carry_into = sum ^ no_carry_sum;
	u16 hc = carry_into & 1 << 4;
	u16 carry = carry_into & 1 << 8;

	u8 res = (u8) sum;
	self->regs[REG_F] = (res == 0 ? F_Z : 0) | (hc ? F_H : 0) | (carry ? F_C : 0);
	self->regs[REG_A] = res;

	return 1;
}

static u8 inst_nop(Cpu*) {
	return 1;
}

static u8 inst_jp(Cpu* self) {
	if (!check_cond(self)) {
		return 1;
	}

	self->pc = self->fetched_data;

	return self->cur_inst->rs == REG_HL ? 1 : 2;
}

static u8 inst_di(Cpu* self) {
	self->ime = false;
	return 1;
}

static u8 inst_or(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;
	u8 res = value | value2;

	self->regs[REG_F] = res == 0 ? F_Z : 0;
	self->regs[REG_A] = res;

	return 1;
}

static u8 inst_and(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;
	u8 res = value & value2;

	self->regs[REG_F] = (res == 0 ? F_Z : 0) | F_H;
	self->regs[REG_A] = res;

	return 1;
}

static u8 inst_rra(Cpu* self) {
	u8 c = (self->regs[REG_F] & F_C) ? 1 : 0;
	u8 value = self->regs[REG_A];
	self->regs[REG_F] = (value & 1) ? F_C : 0;
	self->regs[REG_A] = value >> 1 | c << 7;
	return 1;
}

static u8 inst_adc(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;
	u8 prev_c = (self->regs[REG_F] & F_C) > 0 ? 1 : 0;

	bool hc = ((value & 0xF) + (value2 & 0xF) + prev_c) & 1 << 4;
	u16 res = value + value2 + prev_c;
	u8 trunc_res = res & 0xFF;
	self->regs[REG_F] = (trunc_res == 0 ? F_Z : 0) | (hc ? F_H : 0) | (res > 0xFF ? F_C : 0);
	self->regs[REG_A] = trunc_res;

	return 1;
}

static u8 inst_ei(Cpu* self) {
	self->ime = true;
	return 1;
}

static u8 inst_rlca(Cpu* self) {
	u8 value = self->regs[REG_A];
	self->regs[REG_F] = (value >> 7) ? F_C : 0;
	self->regs[REG_A] = value << 1 | (value >> 7);

	return 1;
}

static u8 inst_sbc(Cpu* self) {
	u8 value = self->regs[REG_A];
	u8 value2 = (u8) self->fetched_data;
	u8 prev_c = (self->regs[REG_F] & F_C) > 0 ? 1 : 0;

	bool hc = ((value & 0xF) - (value2 & 0xF) - prev_c) & 1 << 4;
	u8 res = value - value2 - prev_c;
	bool c = (value2 + prev_c) > value;
	self->regs[REG_F] = (res == 0 ? F_Z : 0) | (hc ? F_H : 0) | (c ? F_C : 0) | F_N;
	self->regs[REG_A] = res;

	return 1;
}

static u8 inst_cpl(Cpu* self) {
	self->regs[REG_A] = ~self->regs[REG_A];
	self->regs[REG_F] = (self->regs[REG_F] & (F_Z | F_C)) | F_H | F_N;
	return 1;
}

static u8 inst_scf(Cpu* self) {
	self->regs[REG_F] = (self->regs[REG_F] & F_Z) | F_C;
	return 1;
}

static u8 inst_ccf(Cpu* self) {
	if (self->regs[REG_F] & F_C) {
		self->regs[REG_F] = self->regs[REG_F] & F_Z;
	}
	else {
		self->regs[REG_F] = (self->regs[REG_F] & F_Z) | F_C;
	}
	return 1;
}

static u8 inst_rrca(Cpu* self) {
	u8 value = self->regs[REG_A];
	self->regs[REG_F] = (value & 1) ? F_C : 0;
	self->regs[REG_A] = value >> 1 | (value << 7);
	return 1;
}

static u8 inst_daa(Cpu* self) {
	u8 flags = self->regs[REG_F];
	u8 a = self->regs[REG_A];

	if (!(flags & F_N)) {
		if ((flags & F_C) || a > 0x99) {
			a += 0x60;
			self->regs[REG_F] |= F_C;
		}
		if ((flags & F_H) || (a & 0xF) > 9) {
			a += 6;
		}
	}
	else {
		if (flags & F_C) {
			a -= 0x60;
		}
		if (flags & F_H) {
			a -= 6;
		}
	}

	self->regs[REG_F] &= ~F_H;
	if (a == 0) {
		self->regs[REG_F] |= F_Z;
	}
	else {
		self->regs[REG_F] &= ~F_Z;
	}

	self->regs[REG_A] = a;

	return 1;
}

static u8 inst_rst(Cpu* self) {
	bus_write(self->bus, --self->sp, self->pc >> 8);
	bus_write(self->bus, --self->sp, self->pc);
	self->pc = self->cur_inst->num * 8;
	return 4;
}

static u8 inst_reti(Cpu* self) {
	self->pc = bus_read(self->bus, self->sp++);
	self->pc |= bus_read(self->bus, self->sp++) << 8;
	self->ime = true;
	return 4;
}

static u8 inst_halt(Cpu* self) {
	self->halted = true;

	return 1;
}

InstFn INST_FNS[T_MAX] = {
	[T_LD] = inst_ld,
	[T_LD16] = inst_ld16,
	[T_XOR] = inst_xor,
	[T_CB] = inst_cb,
	[T_JR] = inst_jr,
	[T_INC] = inst_inc,
	[T_CALL] = inst_call,
	[T_PUSH] = inst_push,
	[T_RLA] = inst_rla,
	[T_POP] = inst_pop,
	[T_DEC] = inst_dec,
	[T_RET] = inst_ret,
	[T_CP] = inst_cp,
	[T_SUB] = inst_sub,
	[T_ADD] = inst_add,
	[T_NOP] = inst_nop,
	[T_JP] = inst_jp,
	[T_DI] = inst_di,
	[T_OR] = inst_or,
	[T_AND] = inst_and,
	[T_RRA] = inst_rra,
	[T_ADC] = inst_adc,
	[T_EI] = inst_ei,
	[T_RLCA] = inst_rlca,
	[T_SBC] = inst_sbc,
	[T_CPL] = inst_cpl,
	[T_SCF] = inst_scf,
	[T_CCF] = inst_ccf,
	[T_RRCA] = inst_rrca,
	[T_DAA] = inst_daa,
	[T_RETI] = inst_reti,
	[T_RST] = inst_rst,
	[T_HALT] = inst_halt
};