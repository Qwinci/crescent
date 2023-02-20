#pragma once
#include "memory/map.hpp"
#include "memory/memory.hpp"
#include "memory/pmm.hpp"
#include "types.hpp"
#include "utils.hpp"

namespace Pci {
	enum class Cmd : u16 {
		IoSpace = 1 << 0,
		MemSpace = 1 << 1,
		BusMaster = 1 << 2,
		SpecialCycles = 1 << 3,
		MemWriteInvalidateEnable = 1 << 4,
		VgaPaletteSnoop = 1 << 5,
		ParityErrorResponse = 1 << 6,
		SerrEnable = 1 << 8,
		FastBackToBackEnable = 1 << 9,
		InterruptDisable = 1 << 10
	};

	constexpr Cmd operator|(Cmd lhs, Cmd rhs) {
		return as<Cmd>(as<u16>(lhs) | as<u16>(rhs));
	}

	constexpr void operator|=(volatile Cmd& lhs, Cmd rhs) {
		lhs = lhs | rhs;
	}

	constexpr u16 operator&(Cmd lhs, Cmd rhs) {
		return as<u16>(lhs) & as<u16>(rhs);
	}

	constexpr void operator&=(volatile Cmd& lhs, Cmd rhs) {
		lhs = as<Cmd>(as<u16>(lhs) & as<u16>(rhs));
	}

	constexpr Cmd operator~(Cmd lhs) {
		return as<Cmd>(~as<u16>(lhs));
	}

	struct CommonHeader {
		u16 vendor_id;
		u16 device_id;
		volatile Cmd command;
		volatile u16 status;
		u8 rev_id;
		u8 prog_if;
		u8 subclass;
		u8 class_code;
		volatile u8 cache_line;
		u8 latency;
		u8 hdr_type;
		volatile u8 bist;
	};

	enum class Cap : u8 {
		PowerManagement = 1,
		Agp = 2,
		Vpd = 3,
		SlotIdent = 4,
		Msi = 5,
		HotSwap = 6,
		PciX = 7,
		HyperTransport = 8,
		Debug = 0xA,
		CentralResourceControl = 0xB,
		HotPlug = 0xC,
		BridgeSubsystemVendorId = 0xD,
		Agp8x = 0xE,
		SecureDevice = 0xF,
		PciExpress = 0x10,
		MsiX = 0x11
	};

	struct Header0 {
		[[nodiscard]] bool has_cap_list() const {
			return common.status & 1 << 4;
		}

		void* get_cap(Cap cap);
		usize get_bar_size(u8 bar);

		[[nodiscard]] inline bool is_io_space(u8 bar) const {
			return bars[bar] & 1;
		}

		[[nodiscard]] usize map_bar(u8 bar);

		[[nodiscard]] inline u32 get_io_bar(u8 bar) const {
			return bars[bar] & ~0b11;
		}

		CommonHeader common;
		volatile u32 bars[6];
		u32 cardbus_cis_ptr;
		u16 subsystem_vendor_id;
		u16 subsystem_id;
		u32 expansion_rom_addr;
		u8 cap_ptr;
		u8 reserved1[3];
		u32 reserved2;
		volatile u8 int_line;
		volatile u8 int_pin;
		u8 min_grant;
		u8 max_latency;
	};

	struct PowerCap {
		[[nodiscard]] inline bool no_soft_reset() const {
			return status_control & 1 << 3;
		}

		enum class State : u8 {
			D0 = 0,
			D1 = 1,
			D2 = 0b10,
			D3 = 0b11
		};

		inline void set_power(State state) {
			status_control &= ~0b11;
			status_control |= as<u8>(state);
		}

	private:
		u8 id;
		u8 next;
		u16 cap_reg;
		volatile u16 status_control;
		u8 pm_control_status_ext;
		u8 data;
	};

	struct MsiCap {
		inline void set_addr(u64 addr) volatile {
			msg_low = as<u32>(addr);
			msg_high = as<u32>(addr >> 32);
		}

		inline void set_data(u8 vec, bool edge_trigger = true, bool deassert = true) volatile {
			msg_data = as<u16>(vec) |
					   (as<u16>(!edge_trigger) << 15) |
					   (as<u16>(!deassert) << 14);
		}

		inline void set_cpu(u8 cpu) volatile {
			set_addr(0xFEE00000 | as<u64>(cpu) << 12);
		}

		inline void enable(bool enable) volatile {
			if (enable) {
				msg_control |= 1;
			}
			else {
				msg_control &= ~(1);
			}
		}

		[[nodiscard]] inline u8 get_max_int_cap() const {
			return 1 << (msg_control >> 1 & 0b111);
		}

		inline void set_max_int(u8 max) volatile {
			auto count = as<u8>(__builtin_clzll(max));
			msg_control &= 0b111 << 4;
			msg_control |= count << 4;
		}

		[[nodiscard]] inline bool get_64bit() const {
			return msg_control & 1 << 7;
		}

		[[nodiscard]] inline bool per_vec_mask() const {
			return msg_control & 1 << 8;
		}

	private:
		u8 id;
		u8 next;
		u16 msg_control;
		u32 msg_low;
		u32 msg_high;
		u16 msg_data;
		u16 reserved;
	public:
		volatile u32 mask;
		volatile u32 pending;
	};
	static_assert(sizeof(MsiCap) == 1 + 1 + 2 + 8 + 2 + 2 + 4 + 4);

	struct MsiXCap {
		struct TableEntry {
			inline void mask(bool mask) {
				if (mask) {
					vector_ctrl &= 1;
				}
				else {
					vector_ctrl |= 1;
				}
			}

			inline void set_data(u8 vec, bool edge_trigger = true, bool deassert = true) {
				msg_data = as<u32>(vec) |
						   (as<u32>(!edge_trigger) << 15) |
						   (as<u32>(!deassert) << 14);
			}

			inline void set_cpu(u8 cpu) {
				msg_addr = 0xFEE00000 | as<u64>(cpu) << 12;
			}

		private:
			u64 msg_addr;
			u32 msg_data;
			u32 vector_ctrl;
		};

		inline void enable(bool enable) {
			if (enable) {
				msg_control |= 1 << 15;
			}
			else {
				msg_control &= ~(1 << 15);
			}
		}

		inline void mask_all(bool mask) {
			if (mask) {
				msg_control |= 1 << 14;
			}
			else {
				msg_control &= ~(1 << 14);
			}
		}

		[[nodiscard]] inline u16 get_table_size() const {
			return (msg_control & 0b11111111111);
		}

		[[nodiscard]] inline u32 get_table_offset() const {
			return (table_off_bir & ~0b111);
		}

		[[nodiscard]] inline u8 get_table_bar() const {
			return (table_off_bir & 0b111);
		}

	private:
		u8 id;
		u8 next;
		u16 msg_control;
		u32 table_off_bir;
		u32 pb_off_bir;
	};
	static_assert(sizeof(MsiXCap) == 1 + 1 + 2 + 4 + 4);
}

void init_pci(void* rsdp);