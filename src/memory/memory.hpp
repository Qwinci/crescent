#pragma once
#include "types.hpp"

class PageAllocator {
public:
	void* alloc(usize count);
	void* alloc_low(usize count);
	void dealloc(void* ptr, usize count);
	void add_memory(usize base, usize size);
private:
	struct Node {
		Node* prev;
		Node* next;
		usize size;
	};

	Node* root;
	Node* end;

	void insert_node(Node* node);
	void remove_node(Node* node);
	void try_merge_node(Node* node);
};

class Allocator {
public:
	void* alloc(usize size);
	void* alloc_low(usize size);
	void dealloc(void* ptr, usize size);
	void dealloc_low(void* ptr, usize size);
	void* realloc(void* ptr, usize old_size, usize size);
	void* realloc_low(void* ptr, usize old_size, usize size);
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
	Freelist low_freelists[9];
};

extern PageAllocator PAGE_ALLOCATOR;
extern Allocator ALLOCATOR;