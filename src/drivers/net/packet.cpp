#include "packet.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"

Packet::Packet() { // NOLINT(cppcoreguidelines-pro-type-member-init)
	begin = as<u8*>(as<void*>(PhysAddr {PAGE_ALLOCATOR.alloc_new()}.to_virt()));
	end = begin;
}

Packet::~Packet() {
	PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {begin}.to_phys().as_usize()));
}

usize Packet::size() const {
	return end - begin;
}

Packet::Packet(const Packet& other) { // NOLINT(cppcoreguidelines-pro-type-member-init)
	*this = other;
}
