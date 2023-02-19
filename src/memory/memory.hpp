#pragma once
#include "types.hpp"

struct Page;

constexpr usize PAGE_SIZE = 0x1000;

class PageAllocator {
public:
	void add_mem(usize base, usize size);
	void* alloc_new(bool lock = true, bool persistent = true);
	void dealloc_new(void* ptr, bool lock = true);
	void* alloc_dma(usize count);
	void dealloc_dma(void* ptr, usize count);
private:
	void* alloc_dma_helper(Page*& page_list, usize count);
	struct Node {
		Node* next;
	};

	Page* dma_freelist {};
	static constexpr usize DMA_HASHTABLE_SIZE = 16;
	Page* dma_move_hashtable[DMA_HASHTABLE_SIZE] {};

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