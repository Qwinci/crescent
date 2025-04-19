#pragma once
#include "inst.h"

typedef Cpu Cpu;

extern const char* INST_NAMES[T_MAX];
void print_inst(Cpu* self, Inst* inst, u16 start_pc);
