#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "utils.hpp"

PageAllocator PAGE_ALLOCATOR;

void PageAllocator::add_mem(usize base, usize size) {
	if (size < PAGE_SIZE) {
		return;
	}

	Bitmap bitmap {};
	auto bitmap_real_size = get_bitmap_real_size(size);
	auto bitmap_size = get_bitmap_size(size - bitmap_real_size - sizeof(BitmapNode));
	auto end = base + size;
	bitmap.init(cast<u64*>(PhysAddr {end - bitmap_real_size}.to_virt().as_usize()), size / PAGE_SIZE);
	end -= bitmap_real_size;

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

	for (usize i = 0; i < bitmap_size; ++i) {
		auto node = new (PhysAddr {base + i * PAGE_SIZE}.to_virt()) Node;
		node->next = root;
		if (root) {
			root->prev = node;
		}
		root = node;
	}
}

void* PageAllocator::alloc_new() {
	if (!root) {
		return nullptr;
	}

	auto node = root;
	root = root->next;
	if (root) {
		root->prev = nullptr;
	}

	auto phys = VirtAddr {node}.to_phys().as_usize();

	for (auto map = bitmap_root; map; map = map->next) {
		if (phys >= map->base && phys <= map->base + map->map.size() * PAGE_SIZE) {
			map->map[(phys - map->base) / PAGE_SIZE] = true;
			break;
		}
	}

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
					auto n = cast<Node*>(PhysAddr {node->base + (i + j) * PAGE_SIZE}.to_virt().as_usize());
					if (n->prev) {
						n->prev->next = n->next;
					}
					else {
						root = n->next;
					}
					if (n->next) {
						n->next->prev = n->prev;
					}
					node->map[i + j] = true;
				}
				return cast<void*>(node->base + i * PAGE_SIZE);
			}
		}
	}

	return nullptr;
}

void PageAllocator::dealloc_new(void* ptr) {
	auto node = new (PhysAddr {ptr}.to_virt()) Node;
	node->next = root;
	node->prev = nullptr;
	if (root) {
		root->prev = node;
	}
	root = node;

	auto phys = cast<usize>(ptr);

	for (auto map = bitmap_root; map; map = map->next) {
		if (phys >= map->base && phys <= map->base + map->map.size() * PAGE_SIZE) {
			map->map[(phys - map->base) / PAGE_SIZE] = false;
			break;
		}
	}
}

void PageAllocator::dealloc_low_new(void* ptr, usize count) {
	auto phys = cast<usize>(ptr);
	auto virt = VirtAddr {ptr};

	for (auto map = bitmap_root; map; map = map->next) {
		if (phys >= map->base && phys <= map->base + map->map.size() * PAGE_SIZE) {
			for (usize i = 0; i < count; ++i) {
				auto node = new (virt.offset(i * PAGE_SIZE)) Node;
				node->next = root;
				node->prev = nullptr;
				if (root) {
					root->prev = node;
				}
				root = node;

				map->map[(phys - map->base) / PAGE_SIZE + i] = false;
			}
			break;
		}
	}
}