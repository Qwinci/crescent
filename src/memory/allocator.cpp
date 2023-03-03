#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "std.hpp"
#include "vmem.hpp"
#include "utils/math.hpp"

Allocator ALLOCATOR;

constexpr u8 size_to_index(u16 size) {
	if (size <= 64) {
		return 0;
	}
	auto log = log2(size);
	log -= 6;
	return log;
}

constexpr u16 index_to_size(u8 index) {
	return pow2(index + 6);
}

extern Spinlock alloc_lock;

void* Allocator::alloc_helper(usize index) { // NOLINT(misc-no-recursion)
	auto& list = freelists[index];

	if (auto node = list.root) {
		list.root = list.root->next;
		return node;
	}

	if (index == FREELIST_COUNT - 1) {
		auto mem = PAGE_ALLOCATOR.alloc_new(false);
		if (!mem) {
			return nullptr;
		}

		auto virt = PhysAddr {mem}.to_virt();

		auto n = new (virt) Freelist::Node;
		list.insert(n);

		auto size = index_to_size(index);
		return virt.offset(as<usize>(size));
	}

	auto next_level_node = alloc_helper(index + 1);
	if (!next_level_node) {
		return nullptr;
	}

	auto n = new (next_level_node) Freelist::Node;
	list.insert(n);

	auto size = index_to_size(index);
	return VirtAddr {next_level_node}.offset(as<usize>(size));
}

void* Allocator::alloc(usize size) {
	auto index = size_to_index(size);
	if (index_to_size(index) == PAGE_SIZE) {
		auto mem = PAGE_ALLOCATOR.alloc_new();
		if (!mem) {
			return nullptr;
		}
		return PhysAddr {mem}.to_virt();
	}
	else if (size > PAGE_SIZE) {
		auto count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		return vm_kernel_alloc_backed(count);
	}

	alloc_lock.lock();

	auto node = alloc_helper(index);

	alloc_lock.unlock();
	return node;
}

void Allocator::dealloc_helper(void* ptr, usize index) { // NOLINT(misc-no-recursion)
	auto i_size = index_to_size(index);
	auto& list = freelists[index];

	auto other = cast<usize>(ptr) ^ i_size;

	Freelist::Node* prev = nullptr;
	Freelist::Node* node = list.root;
	while (node) {
		if (cast<usize>(node) == other) {
			if (prev) {
				prev->next = node->next;
			}
			else {
				list.root = node->next;
			}

			usize lowest_node = other & ~(1ULL << (index + 6));

			if (index == FREELIST_COUNT - 1) {
				PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {lowest_node}.to_phys().as_usize()), false);
				return;
			}

			dealloc_helper(cast<void*>(lowest_node), index + 1);
			return;
		}
		prev = node;
		node = node->next;
	}

	auto new_node = new (ptr) Freelist::Node;
	list.insert(new_node);
}

void Allocator::dealloc(void* ptr, usize size) {
	if (size == 0) {
		return;
	}

	auto index = size_to_index(size);
	if (index_to_size(index) == PAGE_SIZE) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {ptr}.to_phys().as_usize()));
		return;
	}
	else if (size > PAGE_SIZE) {
		auto count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		vm_kernel_dealloc_backed(ptr, count);
		return;
	}

	alloc_lock.lock();

	dealloc_helper(ptr, index);

	alloc_lock.unlock();
}

void* Allocator::realloc(void* ptr, usize old_size, usize size) {
	auto mem = alloc(size);
	if (!mem) {
		return nullptr;
	}
	auto s = old_size < size ? old_size : size;
	memcpy(mem, ptr, s);
	dealloc(ptr, old_size);
	return mem;
}

void* operator new(usize size) {
	return ALLOCATOR.alloc(size);
}

void* operator new[](usize size) {
	return ALLOCATOR.alloc(size);
}

void operator delete(void* ptr, usize size) {
	ALLOCATOR.dealloc(ptr, size);
}

void operator delete[](void* ptr, usize size) {
	ALLOCATOR.dealloc(ptr, size);
}
