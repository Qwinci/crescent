#pragma once
#include "types.hpp"

struct Page;

constexpr usize PAGE_SIZE = 0x1000;

class PageAllocator {
public:
	void* alloc_new();
	void dealloc_new(void* ptr);
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
	struct Node {
		Node* next;
	};

	struct Freelist {
		Node* root;
		Node* end;
		usize len;
	};

	Freelist freelists[9];
};

extern PageAllocator PAGE_ALLOCATOR;
extern Allocator ALLOCATOR;