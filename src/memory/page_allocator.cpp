#include "memory.hpp"
#include "utils.hpp"
#include "new.hpp"

PageAllocator PAGE_ALLOCATOR;

void PageAllocator::try_merge_node(Node* node) {
	if (cast<usize>(node) + node->size * 0x1000 == cast<usize>(node->next)) {
		auto next = node->next;
		node->size += next->size;
		if (next->next) {
			next->next->prev = node;
		}
		else {
			end = node;
		}
		node->next = next->next;
	}

	if (node->prev && cast<usize>(node->prev) + node->prev->size * 0x1000 == cast<usize>(node)) {
		auto prev = node->prev;
		prev->size += node->size;
		if (node->next) {
			node->next->prev = prev;
		}
		else {
			end = prev;
		}
		prev->next = node->next;
	}
}

void* PageAllocator::alloc(usize count) {
	auto node = end;
	while (node) {
		if (node->size >= count) {
			auto remaining = node->size - count;
			if (remaining == 0) {
				remove_node(node);
			}
			else {
				node->size -= count;
			}

			return cast<void*>(cast<usize>(node) + remaining * 0x1000);
		}

		node = node->next;
	}

	return nullptr;
}

void* PageAllocator::alloc_low(usize count) {
	auto node = root;
	while (node) {
		if (node->size >= count) {
			auto remaining = node->size - count;
			remove_node(node);
			if (remaining > 0) {
				auto new_node = new (cast<void*>(cast<usize>(node) + count * 0x1000)) Node();
				new_node->size = remaining;
				insert_node(new_node);
			}

			return node;
		}

		node = node->next;
	}

	return nullptr;
}

void PageAllocator::dealloc(void *ptr, usize count) {
	auto node = new (ptr) Node();
	node->size = count;
	insert_node(node);
}

void PageAllocator::insert_node(Node* node) {
	if (!root) {
		root = node;
		end = node;
		return;
	}

	if (node < root) {
		node->next = root;
		root->prev = node;
		root = node;
		try_merge_node(node);
		return;
	}
	if (node > end) {
		end->next = node;
		node->prev = end;
		end = node;
		try_merge_node(node);
		return;
	}

	auto n = root;
	while (true) {
		auto next = n->next;
		if (node < next) {
			next->prev = node;
			node->next = next;
			n->next = node;
			try_merge_node(node);
			return;
		}
		n = n->next;
	}
}

void PageAllocator::add_memory(usize base, usize size) {
	if (size < 0x1000) {
		return;
	}

	auto count = size / 0x1000;

	auto node = new (cast<void*>(base)) Node();
	node->size = count;
	insert_node(node);
}

void PageAllocator::remove_node(Node* node) {
	if (node->prev) {
		node->prev->next = node->next;
	}
	else {
		root = node->next;
	}

	if (node->next) {
		node->next->prev = node->prev;
	}
	else {
		end = node->prev;
	}
}
