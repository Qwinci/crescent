#pragma once
#include "types.hpp"

struct InterruptCtx;

[[noreturn]] void division_exception(InterruptCtx* ctx);
[[noreturn]] void debug_exception(InterruptCtx* ctx);
[[noreturn]] void nmi_exception(InterruptCtx* ctx);
[[noreturn]] void breakpoint_exception(InterruptCtx* ctx);
[[noreturn]] void overflow_exception(InterruptCtx* ctx);
[[noreturn]] void bound_range_exception(InterruptCtx* ctx);
[[noreturn]] void invalid_op_exception(InterruptCtx* ctx);
[[noreturn]] void device_not_available_exception(InterruptCtx* ctx);
[[noreturn]] void double_fault_exception(InterruptCtx* ctx);
[[noreturn]] void invalid_tss_exception(InterruptCtx* ctx);
[[noreturn]] void seg_not_present_exception(InterruptCtx* ctx);
[[noreturn]] void stack_seg_fault_exception(InterruptCtx* ctx);
[[noreturn]] void gp_fault_exception(InterruptCtx* ctx);
[[noreturn]] void page_fault_exception(InterruptCtx* ctx);
[[noreturn]] void x87_float_exception(InterruptCtx* ctx);
[[noreturn]] void alignment_exception(InterruptCtx* ctx);
[[noreturn]] void machine_check_exception(InterruptCtx* ctx);
[[noreturn]] void simd_float_exception(InterruptCtx* ctx);
[[noreturn]] void virtualization_exception(InterruptCtx* ctx);
[[noreturn]] void control_protection_exception(InterruptCtx* ctx);
[[noreturn]] void hypervisor_injection_exception(InterruptCtx* ctx);
[[noreturn]] void vmm_communication_exception(InterruptCtx* ctx);
[[noreturn]] void security_exception(InterruptCtx* ctx);