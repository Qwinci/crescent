#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "utils.hpp"

PageAllocator PAGE_ALLOCATOR;

void PageAllocator::init_bitmap(usize base, usize max_phys) {
	map.init(cast<u64*>(base + HHDM_OFFSET), max_phys / PAGE_SIZE);
}

void PageAllocator::add_mem(usize base, usize size) {
	if (size < PAGE_SIZE) {
		return;
	}

	auto base_addr = base;

	base = PhysAddr {base}.to_virt().as_usize();

	for (usize i = 0; i < size; i += PAGE_SIZE) {
		auto node = new (cast<void*>(base + i)) Node;
		node->next = root;
		root = node;
		map[(base_addr + i) / PAGE_SIZE] = false;
	}
}

void* PageAllocator::alloc_new() {
	if (!root) {
		return nullptr;
	}

	auto node = root;
	root = root->next;

	return cast<void*>(VirtAddr {node}.to_phys().as_usize());
}

void* PageAllocator::alloc_low_new(usize count) {
	auto max = map.size() > SIZE_4GB ? (SIZE_4GB / PAGE_SIZE) : map.size();
	for (usize i = 0; i < max; ++i) {
		bool found = true;
		for (usize j = 0; j < count; ++j) {
			if (map[i + j]) {
				found = false;
				break;
			}
		}

		if (found) {
			for (usize j = 0; j < count; ++j) {
				map[i + j] = true;
			}
			return cast<void*>(i * PAGE_SIZE);
		}
	}

	return nullptr;
}

void PageAllocator::dealloc_new(void* ptr) {
	auto node = new (PhysAddr {ptr}.to_virt()) Node;
	node->next = root;
	root = node;
}

void PageAllocator::dealloc_low_new(void* ptr, usize count) {
	for (usize i = 0; i < count; ++i) {
		map[cast<usize>(ptr) / PAGE_SIZE + i] = false;
	}
}