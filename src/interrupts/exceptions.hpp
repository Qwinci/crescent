#pragma once
#include "types.hpp"

struct InterruptFrame;

[[noreturn, gnu::interrupt]] void division_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void debug_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void nmi_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void breakpoint_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void overflow_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void bound_range_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void invalid_op_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void device_not_available_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void double_fault_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void invalid_tss_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void seg_not_present_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void stack_seg_fault_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void gp_fault_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void page_fault_exception(InterruptFrame* frame, u64 code);
[[noreturn, gnu::interrupt]] void x87_float_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void alignment_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void machine_check_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void simd_float_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void virtualization_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void control_protection_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void hypervisor_injection_exception(InterruptFrame* frame);
[[noreturn, gnu::interrupt]] void vmm_communication_exception(InterruptFrame* frame, u64);
[[noreturn, gnu::interrupt]] void security_exception(InterruptFrame* frame, u64);