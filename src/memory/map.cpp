#include "map.hpp"

usize HHDM_OFFSET;

void PageMap::map(VirtAddr virt, PhysAddr phys, PageFlags flags) {
	auto virt_addr = virt.as_usize();
	auto phys_addr = phys.as_usize();

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
