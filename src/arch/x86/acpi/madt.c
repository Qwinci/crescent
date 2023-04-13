#include "madt.h"
#include "acpi/acpi.h"
#include "arch/map.h"
#include "arch/x86/dev/io.h"
#include "arch/x86/dev/io_apic.h"
#include "mem/utils.h"
#include "stdio.h"

typedef struct {
	SdtHeader header;
	u32 lapic_addr;
	u32 flags;
} Madt;

extern void io_apic_register_override(u8 irq, u32 global_base, bool active_low, bool level_triggered);
extern u32 io_apic_read(IoApic* self, u8 reg);

void x86_parse_madt() {
	const Madt* madt = (const Madt*) acpi_get_table("APIC");
	if (!madt) {
		return;
	}

	if (madt->flags & 1) {
		out1(0xA1, 0xFF);
		out1(0x21, 0xFF);
	}

	for (u32 i = sizeof(Madt); i < madt->header.length;) {
		u8 type = *offset(madt, const u8*, i);
		i += 1;
		u8 length = *offset(madt, const u8*, i);
		i += 1;

		// io apic
		if (type == 1) {
			// io apic id + reserved
			i += 2;
			u32 io_apic_addr = *offset(madt, const u32*, i);
			i += 4;
			u32 global_int_base = *offset(madt, const u32*, i);
			i += 4;

			if (io_apic_count == MAX_IO_APIC_COUNT) {
				kprintf("[kernel][x86]: warning: there are more than %u io apics\n", MAX_IO_APIC_COUNT);
				continue;
			}
			IoApic* apic = &io_apics[io_apic_count++];
			apic->base = (usize) to_virt(io_apic_addr);
			apic->int_base = global_int_base;

			arch_map_page(KERNEL_MAP, apic->base, io_apic_addr, PF_READ | PF_WRITE | PF_NC | PF_SPLIT);
		}
		// io apic int src override
		else if (type == 2) {
			// bus src
			i += 1;
			u8 irq_src = *offset(madt, const u8*, i);
			i += 1;
			u32 global_int = *offset(madt, const u32*, i);
			i += 4;
			u16 flags = *offset(madt, const u16*, i);
			i += 2;

			bool active_low = flags & 1 << 1;
			bool level_triggered = flags & 1 << 3;

			io_apic_register_override(irq_src, global_int, active_low, level_triggered);
		}
		// io apic nmi int src
		else if (type == 3) {
			// todo
			//u8 nmi_src = *offset(madt, const u8*, i);
			i += 2; // reserved
			//u16 flags = *offset(madt, const u16*, i);
			i += 2;
			//u32 global_int = *offset(madt, const u32*, i);
			i += 4;
		}
		// local apic nmi
		else if (type == 4) {
			// todo
			//u8 acpi_cpu_id = *offset(madt, const u8*, i);
			i += 1;
			//u16 flags = *offset(madt, const u16*, i);
			i += 2;
			//u8 lint = *offset(madt, const u8*, i);
			i += 1;
		}
		else {
			i += length - 2;
		}
	}

	for (usize i = 0; i < io_apic_count; ++i) {
		IoApic* apic = &io_apics[i];
		apic->irq_count = (io_apic_read(apic, 1) >> 16) + 1;
	}
}