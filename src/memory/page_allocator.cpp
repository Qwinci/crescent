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

	Bitmap bitmap {};
	auto bitmap_real_size = get_bitmap_real_size(size);
	auto end = base + size;
	bitmap.init(cast<u64*>(PhysAddr {end - bitmap_real_size}.to_virt().as_usize()), size / PAGE_SIZE);
	end -= bitmap_real_size;

	/*auto base_addr = base;

	base = PhysAddr {base}.to_virt().as_usize();

	for (usize i = 0; i < size; i += PAGE_SIZE) {
		auto node = new (cast<void*>(base + i)) Node;
		node->next = root;
		root = node;
		map[(base_addr + i) / PAGE_SIZE] = false;
	}*/

	end -= sizeof(BitmapNode);
	auto n = new (PhysAddr {end}.to_virt()) BitmapNode;
	n->base = base;
	n->map = bitmap;
	n->next = nullptr;

	if (!bitmap_root) {
		bitmap_root = n;
		bitmap_end = n;
	}
	else {
		if (!bitmap_end) {
			__builtin_unreachable();
		}
		bitmap_end->next = n;
		bitmap_end = n;
	}

	for (usize i = 0; i < size - (bitmap_real_size + sizeof(BitmapNode)); i += PAGE_SIZE) {
		auto node = new (PhysAddr {base + i}.to_virt()) Node;
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

void* PageAllocator::alloc_low_new(usize count) {
	for (auto node = bitmap_root; node; node = node->next) {
		for (usize i = 0; i < node->map.size(); ++i) {
			bool found = true;
			for (usize j = 0; j < count; ++j) {
				if (node->map[i + j]) {
					found = false;
					break;
				}
			}

			if (found) {
				for (usize j = 0; j < count; ++j) {
					node->map[i + j] = true;
				}
				return cast<void*>(node->base + i * PAGE_SIZE);
			}
		}
	}

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