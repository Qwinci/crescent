#pragma once
#include "arch/interrupts.h"

NORETURN IrqStatus ex_div(void* ctx, void*);
NORETURN IrqStatus ex_debug(void* ctx, void*);
NORETURN IrqStatus ex_nmi(void* ctx, void*);
NORETURN IrqStatus ex_breakpoint(void* ctx, void*);
NORETURN IrqStatus ex_overflow(void* ctx, void*);
NORETURN IrqStatus ex_bound_range_exceeded(void* ctx, void*);
NORETURN IrqStatus ex_invalid_op(void* ctx, void*);
NORETURN IrqStatus ex_dev_not_available(void* ctx, void*);
NORETURN IrqStatus ex_df(void* ctx, void*);
NORETURN IrqStatus ex_invalid_tss(void* ctx, void*);
NORETURN IrqStatus ex_seg_not_present(void* ctx, void*);
NORETURN IrqStatus ex_stack_seg_fault(void* ctx, void*);
NORETURN IrqStatus ex_gp(void* ctx, void*);
NORETURN IrqStatus ex_x86_float(void* ctx, void*);
NORETURN IrqStatus ex_align_check(void* ctx, void*);
NORETURN IrqStatus ex_mce(void* ctx, void*);
NORETURN IrqStatus ex_simd(void* ctx, void*);
NORETURN IrqStatus ex_virt(void* ctx, void*);
NORETURN IrqStatus ex_control_protection(void* ctx, void*);
NORETURN IrqStatus ex_hypervisor(void* ctx, void*);
NORETURN IrqStatus ex_vmm_comm(void* ctx, void*);
NORETURN IrqStatus ex_security(void* ctx, void*);

NORETURN IrqStatus ex_pf(void* void_ctx, void*);