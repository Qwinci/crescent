#include "exceptions.hpp"
#include "sched/sched.hpp"
#include "sys/syscalls.hpp"
#include "arch/cpu.hpp"

extern "C" [[gnu::used]] void arch_exception_handler(ExceptionFrame* frame) {
	u8 exception_class = frame->esr_el1 >> 26 & 0b111111;

	auto current = get_current_thread();
	if (current <= (Thread*) 0xFFFF000000000000) {
		panic("current is in lower half");
	}

	const char* reason = "";
	if (exception_class == 0) {
		reason = "undefined instruction";
	}
	else if (exception_class == 0x1) {
		reason = "trapped WF* instruction";
	}
	else if (exception_class == 0x3) {
		reason = "trapped MCR/MRC access";
	}
	else if (exception_class == 0x4) {
		reason = "trapped MCRR/MRRC access";
	}
	else if (exception_class == 0x5) {
		reason = "trapped MCR/MRC access";
	}
	else if (exception_class == 0x6) {
		reason = "trapped LDC/STC access";
	}
	else if (exception_class == 0x7) {
		reason = "trapped SVE/SIMD access";
	}
	else if (exception_class == 0xA) {
		reason = "trapped LD64B/ST64B/ST64BV/ST64BV0";
	}
	else if (exception_class == 0xC) {
		reason = "trapped MRRC access";
	}
	else if (exception_class == 0xD) {
		reason = "branch target exception";
	}
	else if (exception_class == 0xE) {
		reason = "illegal execution state";
	}
	else if (exception_class == 0x11) {
		reason = "SVC executed in AArch32";
	}
	else if (exception_class == 0x15) {
		asm volatile("msr daifclr, #0b10");
		syscall_handler(reinterpret_cast<SyscallFrame*>(frame));
		asm volatile("msr daifset, #0b10");
		return;
	}
	else if (exception_class == 0x18) {
		reason = "trapped MSR/MRS/system instruction";
	}
	else if (exception_class == 0x19) {
		reason = "trapped SVE";
	}
	else if (exception_class == 0x1C) {
		reason = "pointer authentication failure";
	}
	else if (exception_class == 0x20) {
		reason = "EL0 instruction abort";
	}
	else if (exception_class == 0x21) {
		reason = "instruction abort";
	}
	else if (exception_class == 0x22) {
		reason = "PC alignment fault";
	}
	else if (exception_class == 0x24) {
		reason = "EL0 data abort";

		auto stack_base = reinterpret_cast<u64>(current->user_stack_base);
		if (frame->far_el1 >= stack_base && frame->far_el1 < stack_base + PAGE_SIZE) {
			println("[kernel][aarch64]: thread '", current->name, "' hit guard page!");
		}
		else if (current->process->handle_pagefault(frame->far_el1)) {
			return;
		}
	}
	else if (exception_class == 0x25) {
		reason = "data abort";

		if (current->handler_ip) {
			frame->elr_el1 = current->handler_ip;
			frame->sp = current->handler_sp;
			return;
		}
		else if (current->process->handle_pagefault(frame->far_el1)) {
			return;
		}
	}
	else if (exception_class == 0x26) {
		reason = "SP alignment fault";
	}
	else if (exception_class == 0x28) {
		reason = "trapped floating-point from AArch32";
	}
	else if (exception_class == 0x2C) {
		reason = "trapped floating-point from AArch64";
	}
	else if (exception_class == 0x2F) {
		reason = "SError interrupt";
	}
	else if (exception_class == 0x30) {
		reason = "EL0 breakpoint";
	}
	else if (exception_class == 0x31) {
		reason = "breakpoint";
	}
	else if (exception_class == 0x32) {
		reason = "EL0 software step";
	}
	else if (exception_class == 0x33) {
		reason = "software step";
	}
	else if (exception_class == 0x34) {
		reason = "EL0 watchpoint exception";
	}
	else if (exception_class == 0x35) {
		reason = "watchpoint exception";
	}
	else if (exception_class == 0x38) {
		reason = "BKPT instruction in AArch32";
	}
	else if (exception_class == 0x3C) {
		reason = "BRK instruction in AArch64";
	}

	println("[kernel][aarch64]: ", Color::Red, "EXCEPTION: ", reason, ", FAR: 0x", Fmt::Hex, frame->far_el1);
	println("\tat 0x", frame->elr_el1, "\nESR: ", frame->esr_el1, Fmt::Reset, Color::Reset);

	if (current->process->user) {
		println("[kernel][aarch64]: killing user process ", current->process->name);
		current->process->killed = true;
		auto& scheduler = current->cpu->scheduler;
		scheduler.update_schedule();
		scheduler.do_schedule();
		__builtin_trap();
	}

	while (true) {
		asm volatile("wfi");
	}
}
