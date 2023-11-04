#include "dev/pci.h"
#include "arch/x86/cpu.h"

void pci_msi_set(PciMsiCap* self, bool enable, u8 vec, Cpu* cpu) {
	X86Cpu* x86_cpu = container_of(cpu, X86Cpu, common);
	if (enable) {
		self->msg_control |= 1;
	}
	else {
		self->msg_control &= ~1;
	}

	self->msg_data = (u32) vec;
	// level_trigger = 1 << 15
	// assert = 1 << 14

	usize msg_addr = 0xFEE00000 | (usize) x86_cpu->apic_id << 12;
	self->msg_low = msg_addr;
	self->msg_high = msg_addr >> 32;
}

void pci_msix_set_entry(PciMsiXEntry* self, bool mask, u8 vec, Cpu* cpu) {
	X86Cpu* x86_cpu = container_of(cpu, X86Cpu, common);
	if (mask) {
		self->vector_ctrl |= 1;
	}
	else {
		self->vector_ctrl &= ~1;
	}

	self->msg_data = (u32) vec;
	// level_trigger = 1 << 15
	// assert = 1 << 14

	usize msg_addr = 0xFEE00000 | (usize) x86_cpu->apic_id << 12;
	self->msg_addr_low = msg_addr;
	self->msg_addr_high = msg_addr >> 32;
}