#pragma once
#include "types.hpp"

namespace acpi {
	struct [[gnu::packed]] Address {
		u8 space_id;
		u8 reg_bit_width;
		u8 reg_bit_offset;
		u8 reserved;
		u64 address;

		[[nodiscard]] constexpr bool is_memory() const {
			return space_id == 0;
		}
	};

	struct SdtHeader {
		char signature[4];
		u32 length;
		u8 revision;
		u8 checksum;
		char oem_id[6];
		char oem_table_id[8];
		u32 oem_revision;
		char creator_id[4];
		u32 creator_revision;
	};

	struct [[gnu::packed]] Fadt {
		SdtHeader hdr;
		u32 fw_ctrl;
		u32 dsdt;
		u8 reserved0;
		u8 preferred_pm_profile;
		u16 sci_int;
		u32 smi_cmd;
		u8 acpi_enable;
		u8 acpi_disable;
		u8 s4bios_req;
		u8 pstate_cnt;
		u32 pm1a_evt_blk;
		u32 pm1b_evt_blk;
		u32 pm1a_cnt_blk;
		u32 pm1b_cnt_blk;
		u32 pm2_cnt_blk;
		u32 pm_tmr_blk;
		u32 gpe0_blk;
		u32 gpe1_blk;
		u8 pm1_evt_len;
		u8 pm1_cnt_len;
		u8 pm2_cnt_len;
		u8 pm_tmr_len;
		u8 gpe0_blk_len;
		u8 gpe1_blk_len;
		u8 gpe1_base;
		u8 cst_cnt;
		u16 p_lvl2_lat;
		u16 p_lvl3_lat;
		u16 flush_size;
		u16 flush_stride;
		u8 duty_offset;
		u8 duty_width;
		u8 day_alrm;
		u8 mon_alrm;
		u8 century;
		u16 iapc_boot_arch;
		u8 reserved2;
		u32 flags;
		acpi::Address reset_reg;
		u8 reset_value;
		u16 arm_boot_arch;
		u8 fadt_minor_version;
		u64 x_firmware_ctrl;
		u64 x_dsdt;
		Address x_pm1a_evt_blk;
		Address x_pm1b_evt_blk;
		Address x_pm1a_cnt_blk;
		Address x_pm1b_cnt_blk;
		Address x_pm2_cnt_blk;
		Address x_pm_tmr_blk;
		Address x_gpe0_blk;
		Address x_gpe1_blk;
		Address sleep_ctrl_reg;
		Address sleep_sts_reg;
		u64 hypervisor_vendor;
	};

	struct Madt {
		SdtHeader hdr;
		u32 lapic_addr;
		u32 flags;

		static constexpr u32 FLAG_LEGACY_PIC = 1;
	};

	void init(void* rsdp);
	void qacpi_init();
	void* get_table(const char (&signature)[5], u32 index = 0);

	void* get_rsdp();

	void write_to_addr(const Address& addr, u64 value);
	u64 read_from_addr(const acpi::Address& addr);

	extern Fadt* GLOBAL_FADT;
}
