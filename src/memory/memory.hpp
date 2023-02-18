#pragma once
#include "types.hpp"
#include "utils/rb_tree.hpp"

struct Page;

constexpr usize PAGE_SIZE = 0x1000;

class PageAllocator {
public:
	void* alloc_new(bool lock = true);
	void dealloc_new(void* ptr, bool lock = true);
	void add_mem(usize base, usize size);
private:
	struct Node {
		Page* page;
		Node* next;
	};

	Node* root {};
};

class Allocator {
public:
	void* alloc(usize size);
	void dealloc(void* ptr, usize size);
	void* realloc(void* ptr, usize old_size, usize size);
private:
	void* alloc_helper(usize index);
	void dealloc_helper(void* ptr, usize index);

	static constexpr usize FREELIST_COUNT = 6;

	struct Freelist {
		//RBTree<usize, u8> tree {};
		struct Node {
			Node* next;
		};
		Node* root {};

		inline void insert(Node* node) {
			if (!root) {
				node->next = nullptr;
				root = node;
				return;
			}

			auto n = root;
			while (n->next) {
				if (n->next > node) {
					auto old_next = n->next;
					node->next = old_next;
					n->next = node;
					return;
				}
				n = n->next;
			}

			n->next = node;
			node->next = nullptr;
		}
	};

	Freelist freelists[FREELIST_COUNT] {};
};

extern PageAllocator PAGE_ALLOCATOR;
extern Allocator ALLOCATOR;