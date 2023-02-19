#include "map.hpp"
#include "console.hpp"
#include "memory.hpp"
#include "std.hpp"

usize HHDM_OFFSET;

PageMap* kernel_map;

void PageMap::ensure_toplevel_entries() {
	for (usize i = 0; i < 512; ++i) {
		if (!(entries[i].get_flags() & PageFlags::Present)) {
			auto frame = new Entry[512]();
			entries[i].set_flags(PageFlags::Rw | (i < 256 ? PageFlags::Present | PageFlags::User : PageFlags::Present));
			entries[i].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		}
	}
}

void PageMap::map(VirtAddr virt, PhysAddr phys, PageFlags flags, bool split) {
	auto virt_addr = virt.as_usize();

	virt_addr >>= 12;
	auto pt_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pd_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pdp_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pml4_offset = virt_addr & 0x1FF;

	flags = flags | PageFlags::Present;

	bool huge = false;
	if (flags & PageFlags::Huge) {
		flags &= ~PageFlags::Huge;
		huge = true;

		phys = PhysAddr {phys.as_usize() & ~(SIZE_2MB - 1)};
	}
	else {
		phys = PhysAddr {phys.as_usize() & ~(0x1000 - 1)};
	}

	bool user = flags & PageFlags::User;

	Entry* pdp_entry;
	if (entries[pml4_offset].get_flags() & PageFlags::Present) {
		pdp_entry = cast<Entry*>(entries[pml4_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512]();
		entries[pml4_offset].set_flags(PageFlags::Rw | (user ? PageFlags::Present | PageFlags::User : PageFlags::Present));
		entries[pml4_offset].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		pdp_entry = frame;
	}

	Entry* pd_entry;
	if (pdp_entry[pdp_offset].get_flags() & PageFlags::Present) {
		pd_entry = cast<Entry*>(pdp_entry[pdp_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512]();
		pdp_entry[pdp_offset].set_flags(PageFlags::Rw | (user ? PageFlags::Present | PageFlags::User : PageFlags::Present));
		pdp_entry[pdp_offset].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		pd_entry = frame;
	}

	if (huge) {
		auto pd_flags = pd_entry[pd_offset].get_flags();
		if (!(pd_flags & PageFlags::Huge) && pd_flags & PageFlags::Present) {
			auto* pt_entry = cast<Entry*>(pd_entry[pd_offset].get_addr().to_virt().as_usize());
			ALLOCATOR.dealloc(pt_entry, 0x1000);
		}

		pd_entry[pd_offset].set_flags(flags | PageFlags::Huge);
		pd_entry[pd_offset].set_addr(phys);
		refresh_page(virt.as_usize());
		return;
	}

	Entry* pt_entry;

	if ((pd_entry[pd_offset].get_flags() & PageFlags::Huge) && split) {
		auto frame = new Entry[512]();
		auto old_addr = pd_entry[pd_offset].get_addr();
		auto old_flags = pd_entry[pd_offset].get_flags();
		old_flags &= ~PageFlags::Huge;
		pd_entry[pd_offset].set_addr(VirtAddr {frame}.to_phys());
		pd_entry[pd_offset].set_flags(old_flags);
		for (u16 i = 0; i < 512; ++i) {
			frame[i].set_flags(old_flags);
			frame[i].set_addr(old_addr.offset(as<usize>(i) * 0x1000));
			refresh_page(virt.offset(as<usize>(i) * 0x1000).as_usize());
		}
	}
	else if (pd_entry[pd_offset].get_flags() & PageFlags::Huge) {
		pd_entry[pd_offset].set_flags(flags | PageFlags::Huge);
		return;
	}

	if (pd_entry[pd_offset].get_flags() & PageFlags::Present) {
		pt_entry = cast<Entry*>(pd_entry[pd_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512]();
		pd_entry[pd_offset].set_flags(PageFlags::Rw | (user ? PageFlags::Present | PageFlags::User : PageFlags::Present));;
		pd_entry[pd_offset].set_addr(VirtAddr {frame}.to_phys());
		pt_entry = frame;
	}

	pt_entry[pt_offset].set_flags(flags);
	pt_entry[pt_offset].set_addr(phys);
}

void PageMap::unmap(VirtAddr virt, bool huge, bool dealloc) {
	auto virt_addr = virt.as_usize();

	virt_addr >>= 12;
	auto pt_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pd_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pdp_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pml4_offset = virt_addr & 0x1FF;

	Entry* pdp_entry;
	if (entries[pml4_offset].get_flags() & PageFlags::Present) {
		pdp_entry = cast<Entry*>(entries[pml4_offset].get_addr().to_virt().as_usize());
	}
	else {
		return;
	}

	Entry* pd_entry;
	if (pdp_entry[pdp_offset].get_flags() & PageFlags::Present) {
		pd_entry = cast<Entry*>(pdp_entry[pdp_offset].get_addr().to_virt().as_usize());
	}
	else {
		return;
	}

	if (huge) {
		pd_entry[pd_offset].value = 0;
		return;
	}

	Entry* pt_entry;
	if (pd_entry[pd_offset].get_flags() & PageFlags::Present) {
		pt_entry = cast<Entry*>(pd_entry[pd_offset].get_addr().to_virt().as_usize());
	}
	else {
		return;
	}

	pt_entry[pt_offset].value = 0;

	if (!dealloc) {
		return;
	}

	bool can_dealloc = true;
	for (usize i = 0; i < 512; ++i) {
		if (pt_entry[i].get_flags() & PageFlags::Present) {
			can_dealloc = false;
			break;
		}
	}

	if (can_dealloc) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(pd_entry[pd_offset].get_addr().as_usize()));
		refresh_page(pd_entry[pd_offset].get_addr().to_virt().as_usize());
		pd_entry[pd_offset].value = 0;
	}
	else {
		return;
	}

	for (usize i = 0; i < 512; ++i) {
		if (pd_entry[i].get_flags() & PageFlags::Present) {
			can_dealloc = false;
			break;
		}
	}

	if (can_dealloc) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(pdp_entry[pdp_offset].get_addr().as_usize()));
		refresh_page(pdp_entry[pdp_offset].get_addr().to_virt().as_usize());
		pdp_entry[pdp_offset].value = 0;
	}
}

PhysAddr PageMap::virt_to_phys(VirtAddr addr) {
	auto virt_addr = addr.as_usize();

	virt_addr >>= 12;
	auto pt_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pd_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pdp_offset = virt_addr & 0x1FF;
	virt_addr >>= 9;
	auto pml4_offset = virt_addr & 0x1FF;

	Entry* pdp_entry;
	if (entries[pml4_offset].get_flags() & PageFlags::Present) {
		pdp_entry = cast<Entry*>(entries[pml4_offset].get_addr().to_virt().as_usize());
	}
	else {
		return PhysAddr {nullptr};
	}

	Entry* pd_entry;
	if (pdp_entry[pdp_offset].get_flags() & PageFlags::Present) {
		pd_entry = cast<Entry*>(pdp_entry[pdp_offset].get_addr().to_virt().as_usize());
	}
	else {
		return PhysAddr {nullptr};
	}

	Entry* pt_entry;
	if (pd_entry[pd_offset].get_flags() & PageFlags::Present) {
		pt_entry = cast<Entry*>(pd_entry[pd_offset].get_addr().to_virt().as_usize());
	}
	else {
		return PhysAddr {nullptr};
	}

	return pt_entry[pt_offset].get_addr();
}

void PageMap::load() {
	asm volatile("mov cr3, %0" : : "r"(VirtAddr {cast<usize>(this)}.to_phys().as_usize()));
}

void PageMap::refresh_page(usize addr) { // NOLINT(readability-convert-member-functions-to-static)
	asm volatile("invlpg [%0]" : : "r"(addr) : "memory");
}

void PageMap::ensure_kernel_mapping(PhysAddr phys, usize size) {
	usize i = 0;
	auto p = phys.as_usize();
	auto virt = phys.to_virt().as_usize();

	usize align = 0;
	if (p & (0x1000 - 1)) {
		align = p & (0x1000 - 1);
		p &= ~(0x1000 - 1);
	}

	size += align;

	while ((p & (SIZE_2MB - 1)) && i < size) {
		auto addr = VirtAddr {virt + i};
		map(addr, PhysAddr {p + i}, PageFlags::Rw);
		refresh_page(addr.as_usize());
		i += 0x1000;
	}

	while (i < size) {
		auto addr = VirtAddr {virt + i};
		map(addr, PhysAddr {p + i}, PageFlags::Rw | PageFlags::Huge);
		refresh_page(addr.as_usize());
		i += SIZE_2MB;
	}
}
void PageMap::map_multiple(VirtAddr virt, PhysAddr phys, PageFlags flags, usize count, bool split) {
	for (usize i = 0; i < count; ++i) {
		map(virt.offset(i * 0x1000), phys.offset(i * 0x1000), flags, split);
	}
}

PageMap* PageMap::create_high_half_shared() {
	auto map = new PageMap();
	for (usize i = 256; i < 512; ++i) {
		map->entries[i] = entries[i];
	}
	return map;
}

void PageMap::destroy_lower_half() {
	for (usize i = 0; i < 256; ++i) {
		auto addr = entries[i].get_addr();
		if (addr.as_usize() == 0) {
			continue;
		}

		auto* pdp_entry = cast<Entry*>(addr.to_virt().as_usize());
		for (usize pdp_i = 0; pdp_i < 512; ++pdp_i) {
			auto pd_addr = pdp_entry[pdp_i].get_addr();
			if (pd_addr.as_usize() == 0) {
				continue;
			}

			auto* pd_entry = cast<Entry*>(pd_addr.to_virt().as_usize());
			for (usize pd_i = 0; pd_i < 512; ++pd_i) {
				auto pt_addr = pd_entry[pd_i].get_addr();
				if (pt_addr.as_usize() == 0) {
					continue;
				}

				PAGE_ALLOCATOR.dealloc_new(cast<void*>(pt_addr.as_usize()));
			}

			PAGE_ALLOCATOR.dealloc_new(cast<void*>(pd_addr.as_usize()));
		}

		PAGE_ALLOCATOR.dealloc_new(cast<void*>(addr.as_usize()));
	}
}
