#include "console.hpp"
#include "memory.hpp"
#include "new.hpp"
#include "std.hpp"

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
	if (size >= 0x1000) {
		auto count = (size + 0x1000 - 1) / 0x1000;
		return PAGE_ALLOCATOR.alloc_low(count);
	}

	auto index = size_to_index(size);

	auto& list = low_freelists[index];
	if (!list.root) {
		auto mem = PAGE_ALLOCATOR.alloc_low(1);
		if (!mem) {
			panic("memory allocation of ", size, " bytes failed");
		}

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
	if (size >= 0x1000) {
		auto count = (size + 0x1000 - 1) / 0x1000;
		return PAGE_ALLOCATOR.alloc(count);
	}

	auto index = size_to_index(size);

	auto& list = freelists[index];
	if (!list.root) {
		auto mem = PAGE_ALLOCATOR.alloc(1);
		if (!mem) {
			panic("memory allocation of ", size, " bytes failed");
		}

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
	if (size >= 0x1000) {
		auto count = (size + 0x1000 - 1) / 0x1000;
		return PAGE_ALLOCATOR.dealloc(ptr, count);
	}

	auto index = size_to_index(size);
	auto align_size = index_to_size(index);
	auto& list = freelists[index];
	auto node = new (ptr) Node;

	if (list.len + 1 == 0x1000 / align_size) {
		PAGE_ALLOCATOR.dealloc(list.root, 1);
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
	if (size >= 0x1000) {
		auto count = (size + 0x1000 - 1) / 0x1000;
		return PAGE_ALLOCATOR.dealloc(ptr, count);
	}

	auto index = size_to_index(size);
	auto align_size = index_to_size(index);
	auto& list = low_freelists[index];
	auto node = new (ptr) Node;

	if (list.len + 1 == 0x1000 / align_size) {
		PAGE_ALLOCATOR.dealloc(list.root, 1);
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