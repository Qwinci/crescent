#include "pci.hpp"
#include "acpi/acpi.hpp"
#include "acpi/pci.hpp"
#include "mem/mem.hpp"
#include "stdio.hpp"
#include "assert.hpp"
#include "arch/paging.hpp"
#include "mem/vspace.hpp"
#include "x86/irq.hpp"
#include "arch/x86/dev/io_apic.hpp"
#include "arch/cpu.hpp"
#include "utils/driver.hpp"

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

	void* Header0::get_cap(Cap cap, u32 index) {
		auto* ptr = reinterpret_cast<u8*>(this) + (capabilities_ptr & ~0b11);
		while (true) {
			if (*ptr == static_cast<u8>(cap)) {
				if (index-- == 0) {
					return ptr;
				}
			}
			if (!ptr[1]) {
				break;
			}
			ptr = offset(this, u8*, ptr[1]);
		}
		return nullptr;
	}

	static PciAddress ADDRESS {};

	static void enumerate_func(void* base, u32 func) {
		auto* hdr = offset(base, CommonHdr*, static_cast<u64>(func) << 12);

		if (hdr->vendor_id == 0xFFFF) {
			return;
		}

		if (hdr->type != 0) {
			return;
		}

		ADDRESS.function = func;

		auto* hdr0 = reinterpret_cast<Header0*>(hdr);
		auto* dev = new Device {hdr0, ADDRESS};
		for (const auto* driver = DRIVERS_START; driver != DRIVERS_END; ++driver) {
			if (driver->type == Driver::Type::Pci) {
				auto& driver_pci = driver->pci;
				if (driver_pci->match & PciMatch::Class && driver_pci->_class != hdr->class_) {
					continue;
				}
				else if (driver_pci->match & PciMatch::Subclass && driver_pci->subclass != hdr->subclass) {
					continue;
				}
				else if (driver_pci->match & PciMatch::ProgIf && driver_pci->prog_if != hdr->prog_if) {
					continue;
				}
				else if (driver_pci->match & PciMatch::Device) {
					for (auto& match_dev : driver_pci->devices) {
						if (match_dev.vendor == hdr->vendor_id && match_dev.device == hdr->device_id) {
							if (driver_pci->init(*dev)) {
								goto end;
							}
							break;
						}
					}
					continue;
				}
				else if (driver_pci->fine_match) {
					if (driver_pci->fine_match(*dev)) {
						if (driver_pci->init(*dev)) {
							goto end;
						}
						break;
					}
				}
				else {
					if (driver_pci->init(*dev)) {
						goto end;
					}
				}
			}
		}

		delete dev;
		end:
		println(Fmt::Hex, zero_pad(4), hdr->vendor_id, ":", hdr->device_id, Fmt::Reset);
	}

	static void enumerate_dev(void* base, u32 dev) {
		auto* hdr = offset(base, CommonHdr*, static_cast<u64>(dev) << 15);

		if (hdr->vendor_id == 0xFFFF) {
			return;
		}

		ADDRESS.device = dev;

		if (hdr->type & 1 << 7) {
			for (u32 func = 0; func < 8; ++func) {
				enumerate_func(hdr, func);
			}
		}
		else {
			enumerate_func(hdr, 0);
		}
	}

	static void enumerate_bus(void* base, u32 bus) {
		auto* hdr = offset(base, CommonHdr*, static_cast<u64>(bus) << 20);

		ADDRESS.bus = bus;

		for (u32 dev = 0; dev < 32; ++dev) {
			enumerate_dev(hdr, dev);
		}
	}

	namespace {
		Mcfg* GLOBAL_MCFG = nullptr;
	}

	void acpi_init() {
		auto* table = static_cast<Mcfg*>(acpi::get_table("MCFG"));
		assert(table && "no MCFG found");
		GLOBAL_MCFG = table;
	}

	void acpi_enumerate() {
		u16 entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (u32 i = 0; i < entries; ++i) {
			auto entry = GLOBAL_MCFG->entries[i];

			ADDRESS.seg = entry.seg;

			for (u8 bus_i = entry.start; bus_i < entry.end; ++bus_i) {
				enumerate_bus(to_virt<void>(entry.base), bus_i - entry.start);
			}
		}
	}

	void* get_space(u16 seg, u16 bus, u16 dev, u16 func) {
		u16 entries = (GLOBAL_MCFG->hdr.length - sizeof(Mcfg)) / sizeof(Mcfg::Entry);
		for (u32 i = 0; i < entries; ++i) {
			auto entry = GLOBAL_MCFG->entries[i];

			if (entry.seg == seg) {
				void* res = to_virt<void>(entry.base);
				res = offset(res, void*, static_cast<u64>(bus - entry.start) << 20);
				res = offset(res, void*, static_cast<u64>(dev) << 15);
				res = offset(res, void*, static_cast<u64>(func) << 12);
				return res;
			}
		}

		return nullptr;
	}

	Device::Device(Header0* hdr0, PciAddress address) : hdr0 {hdr0}, address {address} {
		msix = static_cast<caps::MsiX*>(hdr0->get_cap(Cap::MsiX, 0));
		msi = static_cast<caps::Msi*>(hdr0->get_cap(Cap::Msi, 0));
	}

	void* Device::map_bar(u8 bar) const {
		usize phys = hdr0->bars[bar];
		assert(!(phys & 1) && "tried to map io space bar");

		auto old = hdr0->common.command;

		enable_io_space(false);
		enable_mem_space(false);

		hdr0->bars[bar] = 0xFFFFFFFF;
		u32 size = ~(hdr0->bars[bar] & ~0xF) + 1;
		hdr0->bars[bar] = phys;

		bool prefetch = phys & 1 << 3;
		bool is_64 = (phys >> 1 & 0b11) == 2;
		CacheMode cache_mode = prefetch ? CacheMode::WriteThrough : CacheMode::Uncached;

		auto* virt = KERNEL_VSPACE.alloc(size);
		assert(virt);

		phys &= ~0xF;
		auto align = phys & 0xFFF;
		phys &= ~0xFFF;
		if (is_64) {
			phys |= static_cast<u64>(hdr0->bars[bar + 1]) << 32;
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

		hdr0->common.command = old;

		return virt;
	}

	u32 Device::alloc_irqs(u32 min, u32 max, IrqFlags flags) {
		if (!irqs.is_empty()) {
			return 0;
		}
		if (max == 0) {
			max = UINT32_MAX;
		}

		auto use_msi = (flags & IrqFlags::Msi) && msi;
		auto use_msix = (flags & IrqFlags::Msix) && msix;

		u32 count;
		if (use_msix) {
			count = (msix->msg_control & 0x7FF) + 1;
		}
		else if (use_msi) {
			count = 1 << (msi->msg_control >> 1 & 0b111);
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
			u8 bar = msix->table_off_bir & 0b111;
			auto mapping = map_bar(bar);
			assert(mapping);
			auto* entries = offset(mapping, MsiXEntry*, msix->table_off_bir & ~0b111);
			msix->mask_all(true);
			msix->enable(true);

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

			msi->msg_control &= ~1;
			msi->msg_data = all_irqs;
			auto id = get_current_thread()->cpu->lapic_id;
			usize msg_addr = 0xFEE00000 | id << 12;
			msi->msg_low = msg_addr;
			msi->msg_high = msg_addr >> 32;

			for (u32 i = 0; i < count; ++i) {
				irqs[i] = all_irqs + i;
			}
		}
		else {
			u32 irq = x86_alloc_irq(1, flags & IrqFlags::Shared);
			assert(irq);

			auto legacy_irq = acpi::get_legacy_pci_irq(address.seg, address.bus, address.device, hdr0->irq_pin);
			assert(legacy_irq);

			IoApicIrqInfo info {
				.delivery = IoApicDelivery::Fixed,
				.polarity = legacy_irq->active_high ? IoApicPolarity::ActiveHigh : IoApicPolarity::ActiveLow,
				.trigger = legacy_irq->edge_triggered ? IoApicTrigger::Edge : IoApicTrigger::Level,
				.vec = static_cast<u8>(irq)
			};
			IO_APIC.register_irq(legacy_irq->gsi, info);

			enable_legacy_irq(false);
			irqs[0] = irq;
		}

		return count;
	}

	void Device::free_irqs() {
		for (u32 i = 0; i < irqs.size(); ++i) {
			x86_dealloc_irq(i, 1, _flags & IrqFlags::Shared);
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
		if (msix && _flags & IrqFlags::Msix) {
			if (enable) {
				msix->msg_control &= ~(1 << 14);
			}
			else {
				msix->msg_control |= 1 << 14;
			}
		}
		else if (msi && _flags & IrqFlags::Msi) {
			if (enable) {
				msi->msg_control |= 1;
			}
			else {
				msi->msg_control &= ~1;
			}
		}
		else {
			enable_legacy_irq(enable);
		}
	}
}
