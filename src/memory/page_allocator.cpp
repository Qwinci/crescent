#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "utils.hpp"
#include "pmm.hpp"

struct PRegionList {
	PRegion* root {};
	PRegion* end {};
};

PageAllocator PAGE_ALLOCATOR;
PRegionList region_list {};

void PageAllocator::add_mem(usize base, usize size) {
	auto virt = PhysAddr {base}.to_virt();

	auto p_count = size / PAGE_SIZE;

	auto region = new (virt) PRegion;
	region->base = base;
	region->p_count = p_count;
	region->next = nullptr;

	if (!region_list.end) {
		region_list.root = region;
		region_list.end = region;
	}
	else {
		region_list.end->next = region;
		region_list.end = region;
	}

	auto used = ALIGNUP(sizeof(PRegion) + p_count * sizeof(Page), PAGE_SIZE);

	for (usize i = 0; i < used / PAGE_SIZE; ++i) {
		auto& page = region->pages[i];
		page.phys = base + i * PAGE_SIZE;
		page.queue = Page::Queue::None;
	}

	for (usize i = used / PAGE_SIZE; i < p_count; ++i) {
		auto& page = region->pages[i];
		page.phys = base + i * PAGE_SIZE;
		page.queue = Page::Queue::Free;
	}

	for (usize i = used; i < size; i += PAGE_SIZE) {
		auto node = new (virt.offset(i)) Node;
		node->next = root;
		root = node;
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

void PageAllocator::dealloc_new(void* ptr) {
	auto node = new (PhysAddr {ptr}.to_virt()) Node;
	node->next = root;
	root = node;
}