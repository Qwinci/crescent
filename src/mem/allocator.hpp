#pragma once
#include "types.hpp"

class Allocator {
public:
	template<typename T>
	constexpr T* alloc() {
		return static_cast<T*>(alloc(sizeof(T)));
	}
	template<typename T>
	constexpr void free(T* ptr) {
		free(ptr, sizeof(T));
	}

	constexpr void* alloc(usize size) {
		auto index = size_to_index(size);
		if (size & (size - 1)) {
			index += 1;
		}
		return alloc_indexed(index, index_to_size(index));
	}
	constexpr void free(void* ptr, usize size) {
		auto index = size_to_index(size);
		if (size & (size - 1)) {
			index += 1;
		}
		return free_indexed(ptr, index);
	}
private:
	void* alloc_indexed(u8 index, usize size);
	void free_indexed(void* ptr, u8 index);

	struct Node {
		Node* next;
	};

	struct Header {
		Header* prev;
		Header* next;
		usize used;
		Node* freelist;
	};

	// 8 16 32 64 128 256 512 1024 2048
	static constexpr u8 FREELIST_COUNT = 9;
	Header* freelists[FREELIST_COUNT] {};

	static constexpr u8 size_to_index(usize size) {
		if (size <= 8) {
			return 0;
		}
		else {
			return static_cast<u8>((sizeof(unsigned long long) * 8) - __builtin_clzll(size - 1)) - 3;
		}
	}

	static constexpr usize index_to_size(u8 index) {
		return 8 << index;
	}
};

extern Allocator ALLOCATOR;