#pragma once
#include "types.hpp"
#include "double_list.hpp"

class Allocator {
public:
	void* alloc(usize size);
	void free(void* ptr, usize size);

	// 16 32 64 128 256 512 1024 2048
	static constexpr usize FREELIST_COUNT = 8;

private:
	struct Node {
		DoubleListHook hook;
	};
	static_assert(sizeof(Node) <= 16);

	struct Header {
		DoubleListHook hook;
		DoubleList<Node, &Node::hook> list {};
		usize count {};
		usize max {};
	};

	DoubleList<Header, &Header::hook> freelists[FREELIST_COUNT] {};

	static constexpr usize size_to_index(usize size);
};

extern Allocator ALLOCATOR;
