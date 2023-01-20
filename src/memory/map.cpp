#include "map.hpp"
#include "console.hpp"

usize HHDM_OFFSET;

void PageMap::map(VirtAddr virt, PhysAddr phys, PageFlags flags) {
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

	Entry* pdp_entry;
	if (entries[pml4_offset].get_flags() & PageFlags::Present) {
		pdp_entry = cast<Entry*>(entries[pml4_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512];
		entries[pml4_offset].set_flags(flags);
		entries[pml4_offset].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		pdp_entry = frame;
	}

	Entry* pd_entry;
	if (pdp_entry[pdp_offset].get_flags() & PageFlags::Present) {
		pd_entry = cast<Entry*>(pdp_entry[pdp_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512];
		pdp_entry[pdp_offset].set_flags(flags);
		pdp_entry[pdp_offset].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		pd_entry = frame;
	}

	if (huge) {
		pd_entry[pd_offset].set_flags(flags | PageFlags::Huge);
		pd_entry[pd_offset].set_addr(phys);
		return;
	}

	Entry* pt_entry;
	if (pd_entry[pd_offset].get_flags() & PageFlags::Present) {
		pt_entry = cast<Entry*>(pd_entry[pd_offset].get_addr().to_virt().as_usize());
	}
	else {
		auto frame = new Entry[512];
		pd_entry[pd_offset].set_flags(flags);
		pd_entry[pd_offset].set_addr(VirtAddr {cast<usize>(frame)}.to_phys());
		pt_entry = frame;
	}

	pt_entry[pt_offset].set_flags(flags);
	pt_entry[pt_offset].set_addr(phys);
}

void PageMap::unmap(VirtAddr virt, bool huge) {
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
}

void PageMap::load() {
	asm volatile("mov cr3, %0" : : "r"(VirtAddr {cast<usize>(this)}.to_phys().as_usize()));
}

void PageMap::refresh_page(usize addr) { // NOLINT(readability-convert-member-functions-to-static)
	asm volatile("invlpg %0" : : "m"(addr));
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
void PageMap::map_multiple(VirtAddr virt, PhysAddr phys, PageFlags flags, usize count) {
	for (usize i = 0; i < count; ++i) {
		map(virt.offset(i * 0x1000), phys.offset(i * 0x1000), flags);
	}
}
