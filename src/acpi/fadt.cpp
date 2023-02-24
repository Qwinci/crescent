#include "fadt.hpp"
#include "common.hpp"
#include "console.hpp"
#include "io.hpp"
#include "memory/map.hpp"
#include "utils.hpp"

enum class AddrSpace : u8 {
	SystemMem = 0,
	SystemIo = 1,
	PciConf = 2,
	EmbeddedController = 3,
	SystemManagementBus = 4,
	SystemCmos = 5,
	PciDeviceBar = 6,
	Ipmi = 7,
	GeneralPurposeIo = 8,
	GenericSerialBus = 9,
	PlatformCommunicationChannel = 0xA
};

enum class AccessSize : u8 {
	Byte1 = 1,
	Bit16 = 2,
	Bit32 = 3,
	Bit64 = 4
};

struct [[gnu::packed]] GenericAddressStructure {
	AddrSpace addr_space;
	u8 bit_width;
	u8 bit_offset;
	u8 access_size;
	u64 addr;
};

struct Fadt {
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
	GenericAddressStructure reset_reg;
	u8 reset_value;
	u8 reserved3[3];
	u64 x_firmware_ctrl;
	u64 x_dsdt;
	GenericAddressStructure x_pm1a_event_block;
	GenericAddressStructure x_pm1b_event_block;
	GenericAddressStructure x_pm1a_control_block;
	GenericAddressStructure x_pm1b_control_block;
	GenericAddressStructure x_pm2_control_block;
	GenericAddressStructure x_pm_timer_block;
	GenericAddressStructure x_gpe0_block;
	GenericAddressStructure x_gpe1_block;
};

static Fadt* fadt {};

void init_fadt(void* rsdp) {
	fadt = cast<Fadt*>(locate_acpi_table(rsdp, "FACP"));
}

void fadt_reset() {
	if (!fadt || !(fadt->flags & 1 << 10)) {
		return;
	}

	if (fadt->reset_reg.addr_space == AddrSpace::SystemIo) {
		out1(fadt->reset_reg.addr, fadt->reset_value);
	}
	else if (fadt->reset_reg.addr_space == AddrSpace::SystemMem) {
		auto mem = PhysAddr {fadt->reset_reg.addr}.to_virt();
		u8* reg = as<u8*>(as<void*>(mem));
		*reg = fadt->reset_value;
	}
}