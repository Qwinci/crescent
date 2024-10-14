#pragma once
#include "mem/register.hpp"
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

	struct Device;

	namespace caps {
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

		struct Pcie {
			u8 id;
			u8 next;
			u16 caps;
			u32 dev_caps;
			volatile u16 dev_ctrl;

			[[nodiscard]] constexpr bool supports_function_reset() const {
				return dev_caps & 1 << 28;
			}

			constexpr void init_function_reset() {
				dev_ctrl |= 1 << 15;
			}
		};
	}

	namespace common {
		inline constexpr BasicRegister<u16> VENDOR_ID {0};
		inline constexpr BasicRegister<u16> DEVICE_ID {2};
		inline constexpr BasicRegister<u16> COMMAND {4};
		inline constexpr BasicRegister<u16> STATUS {6};
		inline constexpr BasicRegister<u8> REVISION {8};
		inline constexpr BasicRegister<u8> PROG_IF {9};
		inline constexpr BasicRegister<u8> SUBCLASS {10};
		inline constexpr BasicRegister<u8> CLASS {11};
		inline constexpr BasicRegister<u8> CACHE_LINE_SIZE {12};
		inline constexpr BasicRegister<u8> LATENCY {13};
		inline constexpr BasicRegister<u8> TYPE {14};
		inline constexpr BasicRegister<u8> BIST {15};
	}

	namespace hdr0 {
		inline constexpr BasicRegister<u32> BARS[] {
			BasicRegister<u32> {16},
			BasicRegister<u32> {20},
			BasicRegister<u32> {24},
			BasicRegister<u32> {28},
			BasicRegister<u32> {32},
			BasicRegister<u32> {36},
		};
		inline constexpr BasicRegister<u32> CARDBUS_CIS_PTR {40};
		inline constexpr BasicRegister<u16> SUBSYSTEM_VENDOR_ID {44};
		inline constexpr BasicRegister<u16> SUBSYSTEM_ID {46};
		inline constexpr BasicRegister<u32> EXPANSION_ROM_ADDR {48};
		inline constexpr BasicRegister<u8> CAPABILITIES_PTR {52};
		inline constexpr BasicRegister<u8> IRQ_LINE {60};
		inline constexpr BasicRegister<u8> IRQ_PIN {61};
		inline constexpr BasicRegister<u8> MIN_GRANT {62};
		inline constexpr BasicRegister<u8> MAX_LATENCY {63};
	}

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

	struct PciAddress {
		u16 seg;
		u8 bus;
		u8 dev;
		u8 func;
	};

	u8 read8(PciAddress addr, u32 offset);
	u16 read16(PciAddress addr, u32 offset);
	u32 read32(PciAddress addr, u32 offset);

	void write8(PciAddress addr, u32 offset, u8 value);
	void write16(PciAddress addr, u32 offset, u16 value);
	void write32(PciAddress addr, u32 offset, u32 value);

	template<typename R>
	[[nodiscard]] inline auto read(PciAddress addr, R reg) {
		using type = typename R::type;

		if constexpr (sizeof(type) == 1) {
			return static_cast<type>(read8(addr, reg.offset));
		}
		else if constexpr (sizeof(type) == 2) {
			return static_cast<type>(read16(addr, reg.offset));
		}
		else if constexpr (sizeof(type) == 4) {
			return static_cast<type>(read32(addr, reg.offset));
		}
		else {
			kstd::unreachable();
		}
	}

	template<typename R>
	inline void write(PciAddress addr, R reg, typename R::bits_type value) {
		using type = typename R::bits_type;

		if constexpr (sizeof(type) == 1) {
			write8(addr, reg.offset, static_cast<u8>(value));
		}
		else if constexpr (sizeof(type) == 2) {
			write16(addr, reg.offset, static_cast<u16>(value));
		}
		else if constexpr (sizeof(type) == 4) {
			write32(addr, reg.offset, static_cast<u32>(value));
		}
		else {
			kstd::unreachable();
		}
	}

	struct Device {
		explicit Device(PciAddress address);

		[[nodiscard]] inline u8 read8(u32 offset) const {
			return pci::read8(addr, offset);
		}

		[[nodiscard]] inline u16 read16(u32 offset) const {
			return pci::read16(addr, offset);
		}

		[[nodiscard]] inline u32 read32(u32 offset) const {
			return pci::read32(addr, offset);
		}

		inline void write8(u32 offset, u8 value) const {
			pci::write8(addr, offset, value);
		}

		void write16(u32 offset, u16 value) const {
			pci::write16(addr, offset, value);
		}

		void write32(u32 offset, u32 value) const {
			pci::write32(addr, offset, value);
		}

		template<typename R>
		[[nodiscard]] inline auto read(R reg) const {
			return pci::read(addr, reg);
		}

		template<typename R>
		inline void write(R reg, typename R::bits_type value) const {
			pci::write(addr, reg, value);
		}

		[[nodiscard]] usize get_bar(u8 bar) const;

		[[nodiscard]] void* map_bar(u8 bar);

		[[nodiscard]] bool is_io_space(u8 bar) const {
			return read(hdr0::BARS[bar]) & 1;
		}

		[[nodiscard]] u32 get_bar_size(u8 bar);

		[[nodiscard]] inline bool is_64bit(u8 bar) const {
			return (read(hdr0::BARS[bar]) >> 1 & 0b11) == 2;
		}

		u32 alloc_irqs(u32 min, u32 max, IrqFlags flags);
		void free_irqs();
		[[nodiscard]] u32 get_irq(u32 index) const;

		void enable_irqs(bool enable);

		inline void enable_legacy_irq(bool enable) {
			auto cmd = read(common::COMMAND);
			if (enable) {
				cmd &= ~(1U << 10);
			}
			else {
				cmd |= 1U << 10;
			}
			write(common::COMMAND, cmd);
		}

		inline void enable_io_space(bool enable) {
			auto cmd = read(common::COMMAND);
			if (enable) {
				cmd |= 1;
			}
			else {
				cmd &= ~1;
			}
			write(common::COMMAND, cmd);
		}

		inline void enable_mem_space(bool enable) {
			auto cmd = read(common::COMMAND);
			if (enable) {
				cmd |= 1 << 1;
			}
			else {
				cmd &= ~(1 << 1);
			}
			write(common::COMMAND, cmd);
		}

		inline void enable_bus_master(bool enable) {
			auto cmd = read(common::COMMAND);
			if (enable) {
				cmd |= 1 << 2;
			}
			else {
				cmd &= ~(1 << 2);
			}
			write(common::COMMAND, cmd);
		}

		[[nodiscard]] u32 get_cap_offset(Cap cap, u32 index) const;

		void set_power_state(PowerState state);

		PciAddress addr;
		u16 vendor_id;
		u16 device_id;

	private:
		u32 msi_cap_offset;
		u32 msix_cap_offset;
		u32 power_cap_offset;
		kstd::vector<u32> irqs;
		IrqFlags _flags {};
	};
}
