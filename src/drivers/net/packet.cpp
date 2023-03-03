#include "packet.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"

Packet::Packet(bool alloc) { // NOLINT(cppcoreguidelines-pro-type-member-init)
	if (alloc) {
		begin = as<u8*>(as<void*>(PhysAddr {PAGE_ALLOCATOR.alloc_new()}.to_virt()));
		end = begin;
	}
	else {
		begin = nullptr;
		end = nullptr;
	}
}

Packet::~Packet() {
	if (begin) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {begin}.to_phys().as_usize()));
	}
}

usize Packet::size() const {
	return end - begin;
}
