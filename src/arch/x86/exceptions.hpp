#pragma once
#include "types.hpp"

struct InterruptCtx;

extern bool full_int_init;

[[noreturn]] void division_exception(InterruptCtx* ctx, void*);
[[noreturn]] void debug_exception(InterruptCtx* ctx, void*);
[[noreturn]] void nmi_exception(InterruptCtx* ctx, void*);
[[noreturn]] void breakpoint_exception(InterruptCtx* ctx, void*);
[[noreturn]] void overflow_exception(InterruptCtx* ctx, void*);
[[noreturn]] void bound_range_exception(InterruptCtx* ctx, void*);
[[noreturn]] void invalid_op_exception(InterruptCtx* ctx, void*);
[[noreturn]] void device_not_available_exception(InterruptCtx* ctx, void*);
[[noreturn]] void double_fault_exception(InterruptCtx* ctx, void*);
[[noreturn]] void invalid_tss_exception(InterruptCtx* ctx, void*);
[[noreturn]] void seg_not_present_exception(InterruptCtx* ctx, void*);
[[noreturn]] void stack_seg_fault_exception(InterruptCtx* ctx, void*);
[[noreturn]] void gp_fault_exception(InterruptCtx* ctx, void*);
[[noreturn]] void page_fault_exception(InterruptCtx* ctx, void*);
[[noreturn]] void x87_float_exception(InterruptCtx* ctx, void*);
[[noreturn]] void alignment_exception(InterruptCtx* ctx, void*);
[[noreturn]] void machine_check_exception(InterruptCtx* ctx, void*);
[[noreturn]] void simd_float_exception(InterruptCtx* ctx, void*);
[[noreturn]] void virtualization_exception(InterruptCtx* ctx, void*);
[[noreturn]] void control_protection_exception(InterruptCtx* ctx, void*);
[[noreturn]] void hypervisor_injection_exception(InterruptCtx* ctx, void*);
[[noreturn]] void vmm_communication_exception(InterruptCtx* ctx, void*);
[[noreturn]] void security_exception(InterruptCtx* ctx, void*);