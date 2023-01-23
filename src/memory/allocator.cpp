#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "std.hpp"
#include "vmem.hpp"

Allocator ALLOCATOR;

constexpr usize pow(usize value, u8 pow) {
	for (usize orig = value; pow > 1; --pow) value *= orig;
	return value;
}

constexpr usize log2(usize value) {
	auto count = __builtin_clzll(value - 1);
	return 8 * sizeof(usize) - count + 1;
}

constexpr u8 size_to_index(u16 size) {
	if (size <= 8) {
		return 0;
	}
	auto log = log2(size);
	log -= 3;
	return log;
}

constexpr u16 index_to_size(u8 index) {
	return pow(2, index + 3);
}

void* Allocator::alloc_low(usize size) {
	if (size == PAGE_SIZE) {
		auto mem = PAGE_ALLOCATOR.alloc_low_new(1);
		if (!mem) {
			return nullptr;
		}
		return PhysAddr {mem}.to_virt();
	}
	else if (size > PAGE_SIZE) {
		auto count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		auto mem = PAGE_ALLOCATOR.alloc_low_new(count);
		if (!mem) {
			return nullptr;
		}
		return PhysAddr {mem}.to_virt();
	}

	auto index = size_to_index(size);

	auto& list = low_freelists[index];
	if (!list.root) {
		auto m = PAGE_ALLOCATOR.alloc_low_new(1);
		if (!m) {
			panic("memory allocation of ", size, " bytes failed");
		}
		void* mem = PhysAddr {m}.to_virt();

		auto align_size = index_to_size(index);

		auto count = 0x1000 / align_size - 2;

		list.root = new (cast<void*>(cast<usize>(mem) + align_size)) Node;
		auto node = list.root;
		for (usize i = 0; i < count; ++i) {
			auto new_node = new (cast<void*>(cast<usize>(node) + align_size)) Node;
			node->next = new_node;
			node = new_node;
		}

		return mem;
	}
	else {
		auto node = list.root;
		list.root = node->next;
		return node;
	}
}

void* Allocator::alloc(usize size) {
	if (size == PAGE_SIZE) {
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

	auto index = size_to_index(size);

	auto& list = freelists[index];
	if (!list.root) {
		auto m = PAGE_ALLOCATOR.alloc_new();
		if (!m) {
			panic("memory allocation of ", size, " bytes failed");
		}
		void* mem = PhysAddr {m}.to_virt();

		auto align_size = index_to_size(index);

		auto count = 0x1000 / align_size - 2;

		list.root = new (cast<void*>(cast<usize>(mem) + align_size)) Node;
		list.len = count + 1;
		auto node = list.root;
		for (usize i = 0; i < count; ++i) {
			auto new_node = new (cast<void*>(cast<usize>(node) + align_size)) Node;
			node->next = new_node;
			node = new_node;
		}

		return mem;
	}
	else {
		auto node = list.root;
		list.root = node->next;
		list.len -= 1;
		return node;
	}
}

void Allocator::dealloc(void* ptr, usize size) {
	if (size == PAGE_SIZE) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {ptr}.to_phys().as_usize()));
	}
	else if (size > PAGE_SIZE) {
		auto count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		vm_kernel_dealloc_backed(ptr, count);
		return;
	}

	auto index = size_to_index(size);
	auto align_size = index_to_size(index);
	auto& list = freelists[index];
	auto node = new (ptr) Node;

	if (list.len + 1 == 0x1000 / align_size) {
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {list.root}.to_phys().as_usize()));
		list.root = nullptr;
		list.len = 0;
		return;
	}

	list.len += 1;

	if (!list.root) {
		list.root = node;
		return;
	}

	auto n = list.root;
	while (n->next) {
		if (n->next > node) {
			node->next = n->next;
			n->next = node;
			return;
		}
		n = n->next;
	}

	n->next = node;
}

void Allocator::dealloc_low(void* ptr, usize size) {
	if (size == PAGE_SIZE) {
		return PAGE_ALLOCATOR.dealloc_low_new(cast<void*>(VirtAddr {ptr}.to_phys().as_usize()), 1);
	}
	else if (size > PAGE_SIZE) {
		auto count = (size + PAGE_SIZE - 1) / PAGE_SIZE;
		PAGE_ALLOCATOR.dealloc_low_new(cast<void*>(VirtAddr {ptr}.to_phys().as_usize()), count);
		return;
	}

	auto index = size_to_index(size);
	auto align_size = index_to_size(index);
	auto& list = low_freelists[index];
	auto node = new (ptr) Node;

	if (list.len + 1 == 0x1000 / align_size) {
		PAGE_ALLOCATOR.dealloc_low_new(cast<void*>(VirtAddr {list.root}.to_phys().as_usize()), 1);
		list.root = nullptr;
		list.len = 0;
		return;
	}

	list.len += 1;

	if (!list.root) {
		list.root = node;
		return;
	}

	auto n = list.root;
	while (n->next) {
		if (n->next > node) {
			node->next = n->next;
			n->next = node;
			return;
		}
		n = n->next;
	}

	n->next = node;
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

void* Allocator::realloc_low(void* ptr, usize old_size, usize size) {
	auto mem = alloc_low(size);
	if (!mem) {
		return nullptr;
	}
	auto s = old_size < size ? old_size : size;
	memcpy(mem, ptr, s);
	dealloc_low(ptr, old_size);
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
