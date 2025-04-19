#pragma once
#include "cpu.h"

typedef struct Inst {
	enum {
		T_NONE,
		T_NOP,
		T_LD,
		T_LD16,
		T_INC,
		T_DEC,
		T_RLCA,
		T_ADD,
		T_RRCA,
		T_STOP,
		T_RLA,
		T_JR,
		T_RRA,
		T_DAA,
		T_CPL,
		T_SCF,
		T_CCF,
		T_HALT,
		T_ADC,
		T_SUB,
		T_SBC,
		T_AND,
		T_XOR,
		T_OR,
		T_CP,
		T_RET,
		T_POP,
		T_JP,
		T_CALL,
		T_PUSH,
		T_RST,
		T_RETI,
		T_DI,
		T_EI,
		T_CB,
		T_MAX
	} type;

	enum {
		M_IMP,
		M_U8,
		M_U16,
		M_R,
		M_MR_R,
		M_MR,
		M_MR_MR,
		M_MR_U8,

		M_MRI_R,
		M_MRD_R,
		M_MRI,
		M_MRD,

		M_R_M_U8,
		M_R_M_U16,
		M_M_U8,
		M_M_U16_R,
		M_SP_I8,
		M_MR8_R,
		M_MR8
	} mode;

	union {
		enum {
			C_NONE,
			C_NZ,
			C_Z,
			C_NC,
			C_C
		} cond;
		u8 num;
	};

	Reg rd;
	Reg rs;
} Inst;

extern Inst INSTRUCTIONS[0xFF + 1];
