#pragma once
#include "arch/interrupts.h"

IrqStatus ex_div(void* ctx, void*);
IrqStatus ex_debug(void* ctx, void*);
IrqStatus ex_nmi(void* ctx, void*);
IrqStatus ex_breakpoint(void* ctx, void*);
IrqStatus ex_overflow(void* ctx, void*);
IrqStatus ex_bound_range_exceeded(void* ctx, void*);
IrqStatus ex_invalid_op(void* ctx, void*);
IrqStatus ex_dev_not_available(void* ctx, void*);
IrqStatus ex_df(void* ctx, void*);
IrqStatus ex_invalid_tss(void* ctx, void*);
IrqStatus ex_seg_not_present(void* ctx, void*);
IrqStatus ex_stack_seg_fault(void* ctx, void*);
IrqStatus ex_gp(void* ctx, void*);
IrqStatus ex_x86_float(void* ctx, void*);
IrqStatus ex_align_check(void* ctx, void*);
IrqStatus ex_mce(void* ctx, void*);
IrqStatus ex_simd(void* ctx, void*);
IrqStatus ex_virt(void* ctx, void*);
IrqStatus ex_control_protection(void* ctx, void*);
IrqStatus ex_hypervisor(void* ctx, void*);
IrqStatus ex_vmm_comm(void* ctx, void*);
IrqStatus ex_security(void* ctx, void*);

IrqStatus ex_pf(void* void_ctx, void*);