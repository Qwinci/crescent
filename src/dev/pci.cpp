#include "pci.hpp"
#include "acpi/acpi.hpp"
#include "acpi/pci.hpp"
#include "arch/cpu.hpp"
#include "arch/paging.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "assert.hpp"
#include "mem/mem.hpp"
#include "mem/vspace.hpp"
#include "mem/portspace.hpp"
#include "stdio.hpp"
#include "utils/driver.hpp"
#include "x86/irq.hpp"

extern Driver DRIVERS_START[];
extern Driver DRIVERS_END[];

namespace pci {
	struct [[gnu::packed]] Mcfg {
		struct Entry {
			u64 base;
			u16 seg;
			u8 start;
			u8 end;
			u32 reserved;
		};

		acpi::SdtHeader hdr;
		u32 reserved[2];
		Entry entries[];
	};

	struct MsiXEntry {
		u32 msg_addr_low;
		u32 msg_addr_high;
		u32 msg_data;
		u32 vector_ctrl;
	};

	static PciAddress ADDRESS {};

	ManuallyDestroy<IrqSpinlock<kstd::vector<Device*>>> DEVICES {};

	namespace {
		Mcfg* GLOBAL_MCFG = nullptr;
		kstd::pair<u32, u32> ALLOCATED_IRQS[4] {};
	}

	void resume_from_suspend() {
		auto guard = DEVICES->lock();
		for (auto device : *guard) {
			for (int i = 0; i < 6; ++i) {
				device->write(hdr0::BARS[i], device->raw_bars[i]);
				assert(device->read(hdr0::BARS[i]) == device->raw_bars[i]);
				device->set_power_state(PowerState::D0);
				device->enable_mem_space(true);
				device->enable_io_space(true);
			}
		}
	}

	static void enumerate_func(u32 func) {
		ADDRESS.func = func;

		u16 vendor_id = read(ADDRESS, common::VENDOR_ID);

		if (vendor_id == 0xFFFF) {
			return;
		}

		u8 type = read(ADDRESS, common::TYPE);

		if ((type & ~(1 << 7)) != 0) {
			return;
		}

		u8 clazz = read(ADDRESS, common::CLASS);
		u8 subclass = read(ADDRESS, common::SUBCLASS);
		u8 prog_if = read(ADDRESS, common::PROG_IF);

		auto* dev = new Device {ADDRESS};

		DEVICES->lock()->push(dev);

		for (const auto* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
			if (driver->type == DriverType::Pci) {
				auto& driver_pci = driver->pci;
				if (driver_pci->match & PciMatch::Class && driver_pci->_class != clazz) {
					continue;
				}
				else if (driver_pci->match & PciMatch::Subclass && driver_pci->subclass != subclass) {
					continue;
				}
				else if (driver_pci->match & PciMatch::ProgIf && driver_pci->prog_if != prog_if) {
					continue;
				}
				else if (driver_pci->match & PciMatch::Device) {
					for (auto& match_dev : driver_pci->devices) {
						if (match_dev.vendor == vendor_id && match_dev.device == dev->device_id) {
							dev->driver_data = match_dev.data;
							if (driver_pci->init(*dev) == InitStatus::Success) {
								goto end;
							}
							break;
						}
					}
					continue;
				}
				else if (driver_pci->fine_match) {
					if (driver_pci->fine_match(*dev)) {
						if (driver_pci->init(*dev) == InitStatus::Success) {
							goto end;
						}
						break;
					}
				}
				else {
					if (driver_pci->init(*dev) == InitStatus::Success) {
						goto end;
					}
				}
			}
		}

		end:
		//println(Fmt::Hex, zero_pad(4), vendor_id, ":", device_id, Fmt::Reset);
	}

	static void enumerate_dev(u32 dev) {
		ADDRESS.dev = dev;
		ADDRESS.func = 0;

		u16 vendor_id = read(ADDRESS, common::VENDOR_ID);

		if (vendor_id == 0xFFFF) {
			return;
		}

		u8 type = read(ADDRESS, common::TYPE);

		if (type & 1 << 7) {
			for (u32 func = 0; func < 8; ++func) {
				enumerate_func(func);
			}
		}
		else {
			enumerate_func(0);
		}
	}

	static void enumerate_bus(u32 bus) {
		ADDRESS.bus = bus;

		for (u32 dev = 0; dev < 32; ++dev) {
			enumerate_dev(dev);
		}
	}

	void acpi_init() {
		GLOBAL_MCFG = static_cast<Mcfg*>(acpi::get_table("MCFG"));
	}

	void acpi_enumerate() {
		if (!GLOBAL_MCFG) {
			enumerate_bus(0);
			return;
		}

		u16 entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (u32 i = 0; i < entries; ++i) {
			auto entry = GLOBAL_MCFG->entries[i];

			ADDRESS.seg = entry.seg;

			for (u8 bus_i = entry.start; bus_i < entry.end; ++bus_i) {
				enumerate_bus(bus_i);
			}
		}
	}

	void* mcfg_get_space(const PciAddress& addr) {
		assert(GLOBAL_MCFG);

		u16 entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (u32 i = 0; i < entries; ++i) {
			auto entry = GLOBAL_MCFG->entries[i];

			if (entry.seg == addr.seg) {
				void* res = to_virt<void>(entry.base);
				res = offset(res, void*, static_cast<u64>(addr.bus) << 20);
				res = offset(res, void*, static_cast<u64>(addr.dev) << 15);
				res = offset(res, void*, static_cast<u64>(addr.func) << 12);
				return res;
			}
		}

		return nullptr;
	}

	Device::Device(PciAddress address) : addr {address} {
		vendor_id = read(common::VENDOR_ID);
		device_id = read(common::DEVICE_ID);
		msix_cap_offset = get_cap_offset(Cap::MsiX, 0);
		msi_cap_offset = get_cap_offset(Cap::Msi, 0);
		power_cap_offset = get_cap_offset(Cap::Power, 0);

		for (int i = 0; i < 6; ++i) {
			raw_bars[i] = read(hdr0::BARS[i]);
		}
	}

	usize Device::get_bar(u8 bar) const {
		usize phys = raw_bars[bar];
		assert(!(phys & 1) && "tried to get io bar address");
		bool is_64 = (phys >> 1 & 0b11) == 2;
		phys &= ~0xF;
		if (is_64) {
			phys |= static_cast<u64>(raw_bars[bar + 1]) << 32;
		}
		return phys;
	}

	void* Device::map_bar(u8 bar) {
		usize phys = raw_bars[bar];
		assert(!(phys & 1) && "tried to map io space bar");

		auto size = get_bar_size(bar);

		bool prefetch = phys & 1 << 3;
		bool is_64 = (phys >> 1 & 0b11) == 2;
		CacheMode cache_mode = prefetch ? CacheMode::WriteThrough : CacheMode::Uncached;

		auto* virt = KERNEL_VSPACE.alloc(size);
		assert(virt);

		phys &= ~0xF;
		auto align = phys & 0xFFF;
		phys &= ~0xFFF;
		if (is_64) {
			phys |= static_cast<u64>(raw_bars[bar + 1]) << 32;
		}

		auto& KERNEL_MAP = KERNEL_PROCESS->page_map;

		for (usize i = 0; i < ALIGNUP(size + align, PAGE_SIZE); i += PAGE_SIZE) {
			assert(KERNEL_MAP.map(
				reinterpret_cast<u64>(virt) + i,
				phys + i,
				PageFlags::Read | PageFlags::Write,
				cache_mode
				));
		}

		return virt;
	}

	u32 Device::get_bar_size(u8 bar) {
		u32 phys = raw_bars[bar];

		assert(!(phys & 1) && "tried to get the size of io space bar");

		auto old = read(common::COMMAND);

		enable_io_space(false);
		enable_mem_space(false);

		write(hdr0::BARS[bar], 0xFFFFFFFF);
		u32 size = ~(read(hdr0::BARS[bar]) & ~0xF) + 1;
		write(hdr0::BARS[bar], phys);

		write(common::COMMAND, old);

		return size;
	}

	u32 Device::alloc_irqs(u32 min, u32 max, IrqFlags flags) {
		if (!irqs.is_empty()) {
			return 0;
		}
		if (max == 0) {
			max = UINT32_MAX;
		}

		auto use_msi = (flags & IrqFlags::Msi) && msi_cap_offset;
		auto use_msix = (flags & IrqFlags::Msix) && msix_cap_offset;

		u32 count;
		if (use_msix) {
			u16 msg_control = read16(msix_cap_offset + 2);
			count = (msg_control & 0x7FF) + 1;
		}
		else if (use_msi) {
			u16 msg_control = read16(msi_cap_offset + 2);
			count = 1 << (msg_control >> 1 & 0b111);
		}
		else {
			count = 1;
		}

		if (count < min) {
			return 0;
		}

		count = kstd::min(count, max);

		_flags = flags;
		irqs.resize(count);
		if (use_msix) {
			u32 table_off_bir = read32(msix_cap_offset + 4);

			u8 bar = table_off_bir & 0b111;
			auto mapping = map_bar(bar);
			assert(mapping);
			auto* entries = offset(mapping, MsiXEntry*, table_off_bir & ~0b111);

			u16 msg_control = read16(msix_cap_offset + 2);
			// enable
			msg_control |= 1 << 15;
			// mask
			msg_control |= 1 << 14;
			write16(msix_cap_offset + 2, msg_control);

			for (u32 i = 0; i < count; ++i) {
				u32 irq = x86_alloc_irq(1, flags & IrqFlags::Shared);
				assert(irq);
				irqs[i] = irq;
				entries[i].msg_data = irq;

				auto id = get_current_thread()->cpu->lapic_id;
				usize msg_addr = 0xFEE00000 | id << 12;
				entries[i].msg_addr_low = msg_addr;
				entries[i].msg_addr_high = msg_addr >> 32;
				entries[i].vector_ctrl &= ~1;
			}
		}
		else if (use_msi) {
			u32 all_irqs = x86_alloc_irq(count, flags & IrqFlags::Shared);
			assert(all_irqs);

			u16 msg_control = read16(msi_cap_offset + 2);
			msg_control &= ~1;
			write16(msi_cap_offset + 2, msg_control);

			auto id = get_current_thread()->cpu->lapic_id;
			usize msg_addr = 0xFEE00000 | id << 12;

			// 64-bit
			if (msg_control & 1 << 7) {
				write32(msi_cap_offset + 4, msg_addr);
				write32(msi_cap_offset + 8, msg_addr >> 32);
				write32(msi_cap_offset + 12, all_irqs);
			}
			else {
				write32(msi_cap_offset + 4, msg_addr);
				write32(msi_cap_offset + 8, all_irqs);
			}

			for (u32 i = 0; i < count; ++i) {
				irqs[i] = all_irqs + i;
			}
		}
		else {
			u8 irq_pin = read(hdr0::IRQ_PIN);

			auto legacy_irq = acpi::get_legacy_pci_irq(addr.seg, addr.bus, addr.dev, irq_pin);
			assert(legacy_irq);
			legacy_no_free = true;

			u32 irq = 0;
			for (auto [gsi, alloc_irq] : ALLOCATED_IRQS) {
				if (gsi == legacy_irq->gsi) {
					irq = alloc_irq;
					break;
				}
			}

			if (irq == 0) {
				irq = x86_alloc_irq(1, flags & IrqFlags::Shared);
				assert(irq);

				IoApicIrqInfo info {
					.delivery = IoApicDelivery::Fixed,
					.polarity = legacy_irq->active_high ? IoApicPolarity::ActiveHigh : IoApicPolarity::ActiveLow,
					.trigger = legacy_irq->edge_triggered ? IoApicTrigger::Edge : IoApicTrigger::Level,
					.vec = static_cast<u8>(irq)
				};
				IO_APIC.register_irq(legacy_irq->gsi, info);

				bool allocated = false;
				for (auto& alloc : ALLOCATED_IRQS) {
					if (!alloc.second) {
						alloc.first = legacy_irq->gsi;
						alloc.second = irq;
						allocated = true;
						break;
					}
				}

				assert(allocated);
			}

			enable_legacy_irq(false);
			irqs[0] = irq;
		}

		return count;
	}

	void Device::free_irqs() {
		if (!legacy_no_free) {
			for (u32 i = 0; i < irqs.size(); ++i) {
				x86_dealloc_irq(i, 1, _flags & IrqFlags::Shared);
			}
		}

		irqs.clear();
		irqs.shrink_to_fit();
	}

	u32 Device::get_irq(u32 index) const {
		if (index < irqs.size()) {
			return irqs[index];
		}
		else {
			return 0;
		}
	}

	void Device::enable_irqs(bool enable) {
		if (msix_cap_offset && _flags & IrqFlags::Msix) {
			u16 msg_control = read16(msix_cap_offset + 2);
			if (enable) {
				msg_control &= ~(1 << 14);
			}
			else {
				msg_control |= 1 << 14;
			}
			write16(msix_cap_offset + 2, msg_control);
		}
		else if (msi_cap_offset && _flags & IrqFlags::Msi) {
			u16 msg_control = read16(msi_cap_offset + 2);
			if (enable) {
				msg_control |= 1;
			}
			else {
				msg_control &= ~1;
			}
			write16(msi_cap_offset + 2, msg_control);
		}
		else {
			enable_legacy_irq(enable);
		}
	}

	static constinit PortSpace IO_SPACE {0xCF8};
	static constexpr BasicRegister<u32> CFG_ADDR {0};
	static constexpr BasicRegister<u32> CFG_DATA {4};

	u8 read8(PciAddress addr, u32 offset) {
		u32 align = offset & 0b11;
		offset &= ~0b11;

		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			return *offset(ptr, volatile u32*, offset) >> (align * 8) & 0xFF;
		}
		else {
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			u32 value = IO_SPACE.load(CFG_DATA);
			return value >> (align * 8) & 0xFF;
		}
	}

	u16 read16(PciAddress addr, u32 offset) {
		assert(offset % 2 == 0);

		u32 align = offset & 0b11;
		offset &= ~0b11;

		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			return *offset(ptr, volatile u32*, offset) >> (align * 8) & 0xFFFF;
		}
		else {
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			u32 value = IO_SPACE.load(CFG_DATA);
			return value >> (align * 8) & 0xFFFF;
		}
	}

	u32 read32(PciAddress addr, u32 offset) {
		assert(offset % 4 == 0);

		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			return *offset(ptr, volatile u32*, offset);
		}
		else {
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			u32 value = IO_SPACE.load(CFG_DATA);
			return value;
		}
	}

	void write8(PciAddress addr, u32 offset, u8 value) {
		u32 align = offset & 0b11;
		offset &= ~0b11;

		auto old = read32(addr, offset);
		old &= ~(0xFF << (align * 8));
		old |= value << (align * 8);
		write32(addr, offset, old);
	}

	void write16(PciAddress addr, u32 offset, u16 value) {
		u32 align = offset & 0b11;
		offset &= ~0b11;

		auto old = read32(addr, offset);
		old &= ~(0xFFFF << (align * 8));
		old |= value << (align * 8);
		write32(addr, offset, old);
	}

	void write32(PciAddress addr, u32 offset, u32 value) {
		assert(offset % 4 == 0);

		if (GLOBAL_MCFG) {
			void* ptr = mcfg_get_space(addr);
			*offset(ptr, volatile u32*, offset) = value;
		}
		else {
			assert(offset <= 0xFF);

			u32 io_addr = 1 << 31;
			io_addr |= addr.bus << 16;
			io_addr |= addr.dev << 11;
			io_addr |= addr.func << 8;
			io_addr |= offset;

			IO_SPACE.store(CFG_ADDR, io_addr);
			IO_SPACE.store(CFG_DATA, value);
		}
	}

	namespace caps::power {
		static constexpr BitField<u16, PowerState> STATE {0, 2};
	}

	u32 Device::get_cap_offset(Cap cap, u32 index) const {
		u16 status = read(common::STATUS);
		if (!(status & 1 << 4)) {
			return 0;
		}

		u8 offset = read(hdr0::CAPABILITIES_PTR) & ~0b11;
		while (true) {
			u8 id = read8(offset);
			u8 next = read8(offset + 1);
			if (id == static_cast<u8>(cap)) {
				if (index-- == 0) {
					return offset;
				}
			}
			if (!next) {
				break;
			}
			offset = next;
		}

		return 0;
	}

	void Device::set_power_state(PowerState state) {
		if (!power_cap_offset) {
			return;
		}

		auto pmcsr = read16(power_cap_offset + 4);
		pmcsr &= ~caps::power::STATE;
		pmcsr |= caps::power::STATE(state);
		write16(power_cap_offset + 4, pmcsr);
	}
}
