#pragma once
#include "types.hpp"
#include "vector.hpp"

namespace pci {
	void acpi_init();
	void acpi_enumerate();

	enum class Cap : u8 {
		Power = 1,
		Agp = 2,
		Vpd = 3,
		SlotIdent = 4,
		Msi = 5,
		HotSwap = 6,
		PciX = 7,
		HyperTransport = 8,
		Vendor = 9,
		Debug = 10,
		CentralResourceControl = 11,
		HotPlug = 12,
		BridgeSubsystemVendorId = 13,
		Agp8x = 14,
		SecureDevice = 15,
		PciExpress = 16,
		MsiX = 17
	};

	enum class PowerState : u8 {
		D0 = 0b00,
		D1 = 0b01,
		D2 = 0b10,
		D3Hot = 0b11
	};

	namespace caps {
		struct Power {
			u8 id;
			u8 next;
			u16 pmc;
			volatile u16 pmcsr;
			u8 pmcsr_bse;
			u8 data;

			constexpr void set_state(PowerState state) {
				auto old = pmcsr;
				old &= ~0b11;
				old |= static_cast<u8>(state);
				pmcsr = old;
			}

			[[nodiscard]] constexpr bool no_soft_reset() const {
				return pmcsr & 1 << 3;
			}
		};

		struct Msi {
			u8 id;
			u8 next;
			u16 msg_control;
			u32 msg_low;
			u32 msg_high;
			u16 msg_data;
			u16 reserved;
			u32 mask;
			volatile u32 pending;

			inline void enable(bool enable) {
				if (enable) {
					msg_control |= 1;
				}
				else {
					msg_control &= ~1;
				}
			}
		};

		struct MsiX {
			u8 id;
			u8 next;
			u16 msg_control;
			u32 table_off_bir;
			u32 pba_off_bir;

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
		};
	}

	struct CommonHdr {
		u16 vendor_id;
		u16 device_id;
		volatile u16 command;
		volatile u16 status;
		u8 revision;
		u8 prog_if;
		u8 subclass;
		u8 class_;
		u8 cache_line_size;
		u8 latency;
		u8 type;
		volatile u8 bist;
	};

	struct Header0 {
		CommonHdr common;
		volatile u32 bars[6];
		u32 cardbus_cis_ptr;
		u16 subsystem_vendor_id;
		u16 subsystem_id;
		u32 expansion_rom_addr;
		u8 capabilities_ptr;
		u8 reserved0;
		u16 reserved1;
		u32 reserved2;
		u8 irq_line;
		u8 irq_pin;
		u8 min_grant;
		u8 max_latency;

		void* get_cap(Cap cap, u32 index);
	};

	enum class IrqFlags {
		None = 1 << 0,
		Legacy = 1 << 0,
		Msi = 1 << 1,
		Msix = 1 << 2,
		Shared = 1 << 3,
		All = 1 << 0 | 1 << 1 | 1 << 2
	};

	constexpr IrqFlags operator|(IrqFlags lhs, IrqFlags rhs) {
		return static_cast<IrqFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
	}
	constexpr bool operator&(IrqFlags lhs, IrqFlags rhs) {
		return static_cast<int>(lhs) & static_cast<int>(rhs);
	}

	struct Device {
		explicit Device(Header0* hdr0);

		[[nodiscard]] void* map_bar(u8 bar) const;

		u32 alloc_irqs(u32 min, u32 max, IrqFlags flags);
		void free_irqs();
		[[nodiscard]] u32 get_irq(u32 index) const;

		void enable_irqs(bool enable);

		inline void enable_legacy_irq(bool enable) const {
			if (enable) {
				hdr0->common.command &= ~(1U << 10);
			}
			else {
				hdr0->common.command |= 1U << 10;
			}
		}

		inline void enable_io_space(bool enable) const {
			if (enable) {
				hdr0->common.command |= 1;
			}
			else {
				hdr0->common.command &= ~1;
			}
		}

		inline void enable_mem_space(bool enable) const {
			if (enable) {
				hdr0->common.command |= 1 << 1;
			}
			else {
				hdr0->common.command &= ~(1 << 1);
			}
		}

		inline void enable_bus_master(bool enable) const {
			if (enable) {
				hdr0->common.command |= 1 << 2;
			}
			else {
				hdr0->common.command &= ~(1 << 2);
			}
		}

		Header0* hdr0;

	private:
		caps::MsiX* msix;
		caps::Msi* msi;
		kstd::vector<u32> irqs;
		IrqFlags _flags {};
	};

	void* get_space(u16 seg, u16 bus, u16 dev, u16 func);
}
