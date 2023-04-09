#pragma once

NORETURN void ex_div(void* ctx, void*);
NORETURN void ex_debug(void* ctx, void*);
NORETURN void ex_nmi(void* ctx, void*);
NORETURN void ex_breakpoint(void* ctx, void*);
NORETURN void ex_overflow(void* ctx, void*);
NORETURN void ex_bound_range_exceeded(void* ctx, void*);
NORETURN void ex_invalid_op(void* ctx, void*);
NORETURN void ex_dev_not_available(void* ctx, void*);
NORETURN void ex_df(void* ctx, void*);
NORETURN void ex_invalid_tss(void* ctx, void*);
NORETURN void ex_seg_not_present(void* ctx, void*);
NORETURN void ex_stack_seg_fault(void* ctx, void*);
NORETURN void ex_gp(void* ctx, void*);
NORETURN void ex_x86_float(void* ctx, void*);
NORETURN void ex_align_check(void* ctx, void*);
NORETURN void ex_mce(void* ctx, void*);
NORETURN void ex_simd(void* ctx, void*);
NORETURN void ex_virt(void* ctx, void*);
NORETURN void ex_control_protection(void* ctx, void*);
NORETURN void ex_hypervisor(void* ctx, void*);
NORETURN void ex_vmm_comm(void* ctx, void*);
NORETURN void ex_security(void* ctx, void*);

NORETURN void ex_pf(void* void_ctx, void*);