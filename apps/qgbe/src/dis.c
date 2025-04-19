#include "dis.h"
#include <assert.h>
#include <stdio.h>

const char* INST_NAMES[T_MAX] = {
	[T_NONE] = "NONE",
	[T_NOP] = "NOP",
	[T_LD] = "LD",
	[T_INC] = "INC",
	[T_DEC] = "DEC",
	[T_RLCA] = "RLCA",
	[T_ADD] = "ADD",
	[T_RRCA] = "RRCA",
	[T_STOP] = "STOP",
	[T_RLA] = "RLA",
	[T_JR] = "JR",
	[T_RRA] = "RRA",
	[T_DAA] = "DAA",
	[T_CPL] = "CPL",
	[T_SCF] = "SCF",
	[T_CCF] = "CCF",
	[T_HALT] = "HALT",
	[T_ADC] = "ADC",
	[T_SUB] = "SUB",
	[T_SBC] = "SBC",
	[T_AND] = "AND",
	[T_XOR] = "XOR",
	[T_OR] = "OR",
	[T_CP] = "CP",
	[T_RET] = "RET",
	[T_POP] = "POP",
	[T_JP] = "JP",
	[T_CALL] = "CALL",
	[T_PUSH] = "PUSH",
	[T_RST] = "RST",
	[T_RETI] = "RETI",
	[T_DI] = "DI",
	[T_EI] = "EI",
	[T_CB] = "CB"
};

static const char* REG_NAMES[REG_MAX] = {
	[REG_A] = "A",
	[REG_F] = "F",
	[REG_B] = "B",
	[REG_C] = "C",
	[REG_D] = "D",
	[REG_E] = "E",
	[REG_H] = "H",
	[REG_L] = "L",
	[REG_AF] = "AF",
	[REG_BC] = "BC",
	[REG_DE] = "DE",
	[REG_HL] = "HL",
	[REG_SP] = "SP"
};

static Reg CB_REGS[] = {
	[0] = REG_B,
	[1] = REG_C,
	[2] = REG_D,
	[3] = REG_E,
	[4] = REG_H,
	[5] = REG_L,
	[6] = REG_HL,
	[7] = REG_A
};

static const char* CB_REG_NAMES[] = {
	[0] = "B",
	[1] = "C",
	[2] = "D",
	[3] = "E",
	[4] = "H",
	[5] = "L",
	[6] = "(HL)",
	[7] = "A"
};

static const char* COND_NAMES[] = {
	[C_NONE] = NULL,
	[C_NZ] = "NZ",
	[C_NC] = "NC",
	[C_Z] = "Z",
	[C_C] = "C"
};

void print_inst(Cpu* self, Inst* inst, u16 start_pc) {
	printf("[%04X]: ", start_pc);

	u16 data = self->fetched_data;

	const char* name = INST_NAMES[inst->type];

	if (inst->type == T_PUSH) {
		printf("PUSH %s\n", REG_NAMES[inst->rs]);
	}
	else if (inst->type == T_POP) {
		printf("POP %s\n", REG_NAMES[inst->rd]);
	}
	else if (inst->type == T_JR) {
		if (inst->cond != C_NONE) {
			printf("JR %s, %04X\n", COND_NAMES[inst->cond], self->pc + (i8) data);
		}
		else {
			printf("JR %04X\n", self->pc + (i8) data);
		}
	}
	else if (inst->type == T_RET && inst->cond != C_NONE) {
		printf("RET %s\n", COND_NAMES[inst->cond]);
	}
	else if (inst->type == T_JP) {
		if (inst->cond != C_NONE) {
			printf("JP %s, %04X\n", COND_NAMES[inst->cond], data);
		}
		else {
			printf("JP %04X\n", data);
		}
	}
	else if (inst->type == T_CALL) {
		if (inst->cond != C_NONE) {
			printf("CALL %s, %04X\n", COND_NAMES[inst->cond], data);
		}
		else {
			printf("CALL %04X\n", data);
		}
	}
	else if (inst->type == T_CB) {
		u8 op = self->fetched_data;
		u8 reg_idx = op & 7;

		if (op <= 0xF) {
			// RLC
			if (op <= 7) {
				printf("RLC %s\n", CB_REG_NAMES[reg_idx]);
			}
			// RRC
			else {
				printf("RRC %s\n", CB_REG_NAMES[reg_idx]);
			}
		}
		else if (op <= 0x1F) {
			// RL
			if (op <= 0x17) {
				printf("RL %s\n", CB_REG_NAMES[reg_idx]);
			}
			// RR
			else {
				printf("RR %s\n", CB_REG_NAMES[reg_idx]);
			}
		}
		else if (op <= 0x2F) {
			// SLA
			if (op <= 0x27) {
				printf("SLA %s\n", CB_REG_NAMES[reg_idx]);
			}
			// SRA
			else {
				printf("SRA %s\n", CB_REG_NAMES[reg_idx]);
			}
		}
		else if (op <= 0x3F) {
			// SWAP
			if (op <= 0x37) {
				printf("SWAP %s\n", CB_REG_NAMES[reg_idx]);
			}
			// SRL
			else {
				printf("SRL %s\n", CB_REG_NAMES[reg_idx]);
			}
		}
		// BIT
		else if (op <= 0x7F) {
			u8 bit = (op - 0x40) >> 3;
			printf("BIT %u, %s\n", bit, CB_REG_NAMES[reg_idx]);
		}
		// RES
		else if (op <= 0xBF) {
			u8 bit = (op - 0x80) >> 3;
			printf("RES %u, %s\n", bit, CB_REG_NAMES[reg_idx]);
		}
		// SET
		else {
			u8 bit = (op - 0xC0) >> 3;
			printf("SET %u, %s\n", bit, CB_REG_NAMES[reg_idx]);
		}
	}
	else if (inst->mode == M_IMP) {
		printf("%s\n", name);
	}
	else if (inst->mode == M_U8) {
		printf("%s %s, %02X\n", name, REG_NAMES[inst->rd], data);
	}
	else if (inst->mode == M_U16) {
		printf("%s %s, %04X\n", name, REG_NAMES[inst->rd], data);
	}
	else if (inst->mode == M_R) {
		printf("%s %s, %s\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MR_R) {
		printf("%s (%s), %s\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MR) {
		printf("%s %s, (%s)\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MR_MR) {
		printf("%s (%s)\n", name, REG_NAMES[inst->rd]);
	}
	else if (inst->mode == M_MR_U8) {
		printf("%s (%s), %02X\n", name, REG_NAMES[inst->rd], data);
	}
	else if (inst->mode == M_MRI_R) {
		printf("%s (%s+), %s\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MRD_R) {
		printf("%s (%s-), %s\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MRI) {
		printf("%s %s, (%s+)\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MRD) {
		printf("%s %s, (%s-)\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_R_M_U8) {
		printf("%s %s, (FF00 + %02X)\n", name, REG_NAMES[inst->rd], self->dest_addr - 0xFF00);
	}
	else if (inst->mode == M_R_M_U16) {
		printf("%s %s, (%04X)\n", name, REG_NAMES[inst->rd], data);
	}
	else if (inst->mode == M_M_U8) {
		printf("%s (FF00 + %02X), %s\n", name, self->dest_addr - 0xFF00, REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_M_U16_R) {
		printf("%s (%04X), %s\n", name, self->dest_addr, REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_SP_I8) {
		printf("%s %s, SP + %02d\n", name, REG_NAMES[inst->rd], (i8) data);
	}
	else if (inst->mode == M_MR8_R) {
		printf("%s (FF00 + %s), %s\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else if (inst->mode == M_MR8) {
		printf("%s %s, (FF00 + %s)\n", name, REG_NAMES[inst->rd], REG_NAMES[inst->rs]);
	}
	else {
		printf("%u\n", inst->mode);
		assert(false && "print_inst unimplemented mode");
	}
}
