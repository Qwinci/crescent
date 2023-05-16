#include "lapic.h"
#include "arch/interrupts.h"
#include "arch/map.h"
#include "arch/misc.h"
#include "arch/x86/cpu.h"
#include "assert.h"
#include "mem/utils.h"
#include "stdio.h"
#include "types.h"

static usize LAPIC_BASE = 0;

u32 lapic_read(LapicReg reg) {
	return *(volatile u32*) (LAPIC_BASE + reg);
}

void lapic_write(LapicReg reg, u32 value) {
	*(volatile u32*) (LAPIC_BASE + reg) = value;
}

void lapic_first_init() {
	usize phys = x86_get_msr(MSR_IA32_APIC_BASE) & ~0xFFF;
	arch_map_page(KERNEL_MAP, (usize) to_virt(phys), phys, PF_READ | PF_WRITE | PF_NC);
	LAPIC_BASE = (usize) to_virt(phys);
}

void lapic_init() {
	lapic_write(LAPIC_REG_SPURIOUS_INT, 0xFF | 0x100);
	lapic_write(LAPIC_REG_TASK_PRIORITY, 0);
}

void lapic_eoi() {
	lapic_write(LAPIC_REG_EOI, 0);
}

static LapicMsg g_msg;
static Spinlock msg_lock = {};
static u8 ipi_vec = 0;

void ipi_handler(void*, void*) {
	X86Cpu* cpu = x86_get_cur_cpu();
	switch (g_msg) {
		case LAPIC_MSG_HALT:
			spinlock_lock(&cpu->common.lock);
			spinlock_unlock(&cpu->common.lock);
			break;
		case LAPIC_MSG_PANIC:
			panic("received panic msg on cpu %u\n", cpu->apic_id);
	}
}

void lapic_ipi(u8 id, LapicMsg msg) {
	spinlock_lock(&msg_lock);
	if (!ipi_vec) {
		ipi_vec = arch_alloc_int(1, (IntHandler) ipi_handler, NULL);
		assert(ipi_vec);
	}
	g_msg = msg;

	lapic_write(LAPIC_REG_INT_CMD_BASE + 0x10, (u32) id << 24);
	u32 value = ipi_vec;
	lapic_write(LAPIC_REG_INT_CMD_BASE, value);

	while (lapic_read(LAPIC_REG_INT_CMD_BASE) & 1 << 12) {
		arch_spinloop_hint();
	}

	spinlock_unlock(&msg_lock);
}

void lapic_ipi_all(LapicMsg msg) {
	spinlock_lock(&msg_lock);
	if (!ipi_vec) {
		ipi_vec = arch_alloc_int(1, (IntHandler) ipi_handler, NULL);
		assert(ipi_vec);
	}
	g_msg = msg;

	u32 value = ipi_vec | 3 << 18;
	lapic_write(LAPIC_REG_INT_CMD_BASE, value);

	while (lapic_read(LAPIC_REG_INT_CMD_BASE) & 1 << 12) {
		arch_spinloop_hint();
	}

	spinlock_unlock(&msg_lock);
}