#pragma once
#include "types.hpp"
#include "string_view.hpp"

namespace vbt {
	struct Header {
		char signature[20];
		u16 version;
		u16 header_size;
		u16 vbt_size;
		u8 vbt_checksum;
		u8 reserved0;
		u32 bdb_offset;
		u32 aim_offset[4];
	};

	inline constexpr kstd::string_view VBT_SIGNATURE {"$VBT"};
	inline constexpr kstd::string_view BDB_SIGNATURE {"BIOS_DATA_BLOCK "};

	struct [[gnu::packed]] BdbHeader {
		char signature[16];
		u16 version;
		u16 header_size;
		u16 bdb_size;
	};

	enum class BdbBlockId : u8 {
		GeneralDefinitions = 2,
		DriverFeatures = 12,
		MipiSequence = 53
	};

	struct [[gnu::packed]] CommonBdbHeader {
		u8 id;
		u16 size;
	};

	struct [[gnu::packed]] BdbGeneralDefinitions {
		CommonBdbHeader common;
		u8 crt_ddc_gmbus_pin;
		u8 dpms_non_acpi : 1;
		u8 skip_boot_crt_detect : 1;
		u8 dpms_aim : 1;
		u8 reserved : 5;
		u8 boot_display[2];
		u8 child_dev_size;
		u8 devices[];
	};

	struct [[gnu::packed]] BdbDriverFeatures {
		CommonBdbHeader common;
		u8 flags0;
		u16 boot_mode_x;
		u16 boot_mode_y;
		u8 boot_mode_bpp;
		u8 boot_mode_refresh;
		u8 flags1;
		u8 preserve_aspect_ratio : 1;
		u8 sdvo_device_power_down : 1;
		u8 crt_hotplug : 1;
		u8 lvds_config : 2;
		u8 tv_hotplug : 1;
		u8 hdmi_config : 2;
	};

	inline constexpr u16 DEVICE_TYPE_HDMI = 0x60D2;
	inline constexpr u16 DEVICE_TYPE_DP_DUAL_MODE = 0x60D6;
	inline constexpr u16 DEVICE_TYPE_DP = 0x68C6;

	inline constexpr u16 DEVICE_TYPE_DISPLAYPORT_OUTPUT = 1 << 2;

	inline constexpr u8 DVO_PORT_HDMIA = 0;
	inline constexpr u8 DVO_PORT_HDMIB = 1;
	inline constexpr u8 DVO_PORT_HDMIC = 2;
	inline constexpr u8 DVO_PORT_HDMID = 3;
	inline constexpr u8 DVO_PORT_LVDS = 4;
	inline constexpr u8 DVO_PORT_TV = 5;
	inline constexpr u8 DVO_PORT_CRT = 6;
	inline constexpr u8 DVO_PORT_DPB = 7;
	inline constexpr u8 DVO_PORT_DPC = 8;
	inline constexpr u8 DVO_PORT_DPD = 9;
	inline constexpr u8 DVO_PORT_DPA = 10;
	inline constexpr u8 DVO_PORT_DPE = 11;
	inline constexpr u8 DVO_PORT_HDMIE = 12;
	inline constexpr u8 DVO_PORT_DPF = 13;
	inline constexpr u8 DVO_PORT_HDMIF = 14;
	inline constexpr u8 DVO_PORT_DPG = 15;
	inline constexpr u8 DVO_PORT_HDMIG = 16;
	inline constexpr u8 DVO_PORT_DPH = 17;
	inline constexpr u8 DVO_PORT_HDMIH = 18;
	inline constexpr u8 DVO_PORT_DPI = 19;
	inline constexpr u8 DVO_PORT_HDMII = 20;
	inline constexpr u8 DVO_PORT_MIPIA = 21;
	inline constexpr u8 DVO_PORT_MIPIB = 22;
	inline constexpr u8 DVO_PORT_MIPIC = 23;
	inline constexpr u8 DVO_PORT_MIPID = 24;

	struct [[gnu::packed]] ChildDevice {
		u16 handle;
		u16 device_type;

		union [[gnu::packed]] {
			char device_id[10];

			struct [[gnu::packed]] {
				u8 i2c_speed;
				u8 dp_onboard_redriver;
				u8 dp_ondock_redriver;
				u8 hdmi_level_shifter_value : 5;
				u8 hdmi_max_data_rate : 3;
				u16 dtd_buf_ptr;
				u8 edidless_efp : 1;
				u8 compression_enable : 1;
				u8 compression_method_cps : 1;
				u8 ganged_edp : 1;
				u8 lttpr_non_transparent : 1;
				u8 disable_compression_for_ext_disp : 1;
				u8 reserved0 : 2;
				u8 compression_structure_index : 4;
				u8 reserved1 : 4;
				u8 hdmi_max_frl_rate : 4;
				u8 hdmi_max_frl_rate_valid : 1;
				u8 reserved2 : 3;
				u8 reserved3;
			};
		};

		u16 addin_offset;
		u8 dvo_port;
		u8 i2c_pin;
		u8 slave_addr;
		u8 ddc_pin;
		u16 edid_ptr;
		u8 dvo_cfg;

		union [[gnu::packed]] {
			struct [[gnu::packed]] {
				u8 dvo2_port;
				u8 i2c2_pin;
				u8 slave2_addr;
				u8 ddc2_pin;
			};

			struct [[gnu::packed]] {
				u8 efp_routed : 1;
				u8 lane_reversal : 1;
				u8 lspcon : 1;
				u8 iboost : 1;
				u8 hpd_invert : 1;
				u8 use_vbt_vswing : 1;
				u8 dp_max_lane_count : 2;
				u8 hdmi_support : 1;
				u8 dp_support : 1;
				u8 tmds_support : 1;
				u8 support_reserved : 5;
				u8 aux_channel;
				u8 dongle_detect;
			};
		};

		u8 pipe_cap : 2;
		u8 sdvo_stall : 1;
		u8 hpd_status : 2;
		u8 integrated_encoder : 1;
		u8 capabilities_reserved : 2;
		u8 dvo_wiring;

		union [[gnu::packed]] {
			u8 dvo2_wiring;
			u8 mipi_bridge_type;
		};

		u16 extended_type;
		u8 dvo_function;
		u8 dp_usb_type_c : 1;
		u8 tbt : 1;
		u8 flags0_reserved : 2;
		u8 dp_port_trace_length : 4;
		u8 dp_gpio_index;
		u16 dp_gpio_pin_num;
		u8 dp_iboost_level : 4;
		u8 hdmi_iboost_level : 4;
		u8 dp_max_link_rate : 3;
		u8 dp_max_link_rate_reserved : 5;
		u8 efp_index;
	};
}
