#include "fadt.h"
#include "acpi/acpi.h"
#include "arch/misc.h"
#include "mem/utils.h"
#include "stdio.h"
#include "arch/x86/dev/io.h"

typedef struct {
	SdtHeader header;
	u32 fw_ctrl;
	u8 reserved;
	u8 preferred_power_management_profile;
	u16 sci_int;
	u32 smi_cmd_port;
	u8 acpi_enable;
	u8 acpi_disable;
	u8 s4bios_req;
	u8 pstate_ctrl;
	u32 pm1a_event_block;
	u32 pm1b_event_block;
	u32 pm1a_control_block;
	u32 pm1b_control_block;
	u32 pm2_control_block;
	u32 pm_timer_block;
	u32 gpe0_block;
	u32 gpe1_block;
	u8 pm1_event_length;
	u8 pm1_control_length;
	u8 pm2_control_length;
	u8 pm_timer_length;
	u8 gpe0_length;
	u8 gpe1_length;
	u8 gpe1_base;
	u8 c_state_ctrl;
	u16 worst_c2_latency;
	u16 worst_c3_latency;
	u16 flush_size;
	u16 flush_stride;
	u8 duty_offset;
	u8 duty_width;
	u8 day_alarm;
	u8 month_alarm;
	u8 century;
	u16 boot_arch_flags;
	u8 reserved2;
	u32 flags;
	AcpiAddr reset_reg;
	u8 reset_value;
	u8 reserved3[3];
	u64 x_firmware_ctrl;
	u64 x_dsdt;
	AcpiAddr x_pm1a_event_block;
	AcpiAddr x_pm1b_event_block;
	AcpiAddr x_pm1a_control_block;
	AcpiAddr x_pm1b_control_block;
	AcpiAddr x_pm2_control_block;
	AcpiAddr x_pm_timer_block;
	AcpiAddr x_gpe0_block;
	AcpiAddr x_gpe1_block;
} Fadt;

static bool is_io = false;
static u64 reset_reg = 0;
static u8 reset_value = 0;

void x86_parse_fadt(void* rsdp) {
	const Fadt* fadt = (const Fadt*) acpi_get_table("FACP");
	if (!fadt) {
		return;
	}

	// ps2
	if (fadt->boot_arch_flags & 1 << 1) {
		// todo
	}

	if (fadt->boot_arch_flags & 1 << 3) {
		panic("msi is not supported");
	}

	if (fadt->flags & 1 << 10) {
		is_io = fadt->reset_reg.addr_space_id == ACPI_ADDR_SPACE_SYS_IO;
		reset_reg = fadt->reset_reg.address;
		reset_value = fadt->reset_value;
	}
}

typedef struct [[gnu::packed]] {
	u16 limit;
	u64 base;
} Idtr;

void arch_reboot() {
	if (reset_reg) {
		if (is_io) {
			out1(reset_reg, reset_value);
		}
		else {
			u8* reg = (u8*) to_virt(reset_reg);
			*reg = reset_value;
		}
	}

	Idtr idtr = {.limit = 0x1000 - 1};
	__asm__ volatile("lidt %0" : : "m"(idtr));
}