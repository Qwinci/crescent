#include "stdlib.h"
#include "stdio.h"
#include "sys.hpp"
#include "string.h"
#include <new>

// 8 16 32 64 128 256 512 1024 2048 4096 8192
static constexpr size_t SIZE_COUNT = 11;
static constexpr size_t MAX_SIZE = 8192;

static constexpr size_t size_to_index(size_t size) {
	if (size <= 8) {
		return 0;
	}
	return 64 - __builtin_clzll(size - 1) - 3;
}

static constexpr size_t index_to_size(size_t index) {
	return size_t {1} << (3 + index);
}

static constexpr size_t ARENA_SIZE = 1024 * 64;
static constexpr uint32_t ALLOC_MAGIC = 0xCAFEBABE;

struct Header {
	union {
		struct {
			size_t size;
			struct Arena* arena;
		};
		Header* next;
	};
	enum {
		Normal,
		Large
	} type;
	uint32_t magic;
};

struct Arena {
	Header* freelist {};
	size_t total_count {};
	size_t num_free {};
	Arena* prev {};
	Arena* next {};
};

namespace {
	Arena* FREE_ARENAS[SIZE_COUNT];
}

void* malloc(size_t size) {
	if (size > MAX_SIZE) {
		void* mem = nullptr;
		auto ret = sys_map(&mem, size + sizeof(Header), CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
		if (ret != 0) {
			return nullptr;
		}

		auto* hdr = new (mem) Header {
			.size = size + sizeof(Header),
			.type = Header::Large,
			.magic = ALLOC_MAGIC
		};
		return &hdr[1];
	}

	auto index = size_to_index(size);
	auto& arena = FREE_ARENAS[index];

	if (!arena) {
		void* mem = nullptr;
		auto ret = sys_map(&mem, ARENA_SIZE, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
		if (ret != 0) {
			return nullptr;
		}

		size_t block_size = sizeof(Header) + index_to_size(index);
		size_t block_count = (ARENA_SIZE - sizeof(Arena)) / block_size;

		auto* hdr_base = reinterpret_cast<Header*>(reinterpret_cast<uintptr_t>(mem) + sizeof(Arena));
		Header* prev = nullptr;
		for (size_t i = 0; i < block_count; ++i) {
			auto* hdr = new (reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(hdr_base) + i * block_size)) Header {};
			if (prev) {
				prev->next = hdr;
			}
			prev = hdr;
		}

		auto* new_arena = new (mem) Arena {
			.freelist = hdr_base,
			.total_count = block_count,
			.num_free = block_count
		};
		arena = new_arena;
	}

	auto* hdr = arena->freelist;
	arena->freelist = hdr->next;
	--arena->num_free;
	if (!arena->num_free) {
		if (arena->prev) {
			arena->prev->next = arena->next;
		}
		else {
			FREE_ARENAS[index] = arena->next;
		}
		if (arena->next) {
			arena->next->prev = nullptr;
		}
	}

	hdr->size = size;
	hdr->arena = arena;
	hdr->magic = ALLOC_MAGIC;
	return &hdr[1];
}

void* realloc(void* old, size_t new_size) {
	if (old) {
		auto* hdr = static_cast<Header*>(old) - 1;
		if (hdr->magic != ALLOC_MAGIC) {
			puts("invalid allocation magic");
			__builtin_trap();
		}

		auto old_index = size_to_index(hdr->size);
		if (old_index == size_to_index(new_size)) {
			hdr->size = new_size;
			return old;
		}
	}

	if (!new_size) {
		free(old);
		return nullptr;
	}
	void* ptr = malloc(new_size);
	if (!ptr) {
		return nullptr;
	}
	if (old) {
		auto* hdr = static_cast<Header*>(old) - 1;
		memcpy(ptr, old, new_size < hdr->size ? new_size : hdr->size);
		free(old);
	}
	return ptr;
}

void free(void* ptr) {
	if (!ptr) {
		return;
	}

	auto* hdr = static_cast<Header*>(ptr) - 1;
	if (hdr->magic != ALLOC_MAGIC) {
		puts("invalid allocation magic");
		__builtin_trap();
	}
	hdr->magic = 0;

	if (hdr->type == Header::Large) {
		sys_unmap(hdr, hdr->size);
	}
	else {
		auto index = size_to_index(hdr->size);

		auto* arena = hdr->arena;
		if (!arena->num_free) {
			arena->prev = nullptr;
			arena->next = FREE_ARENAS[index];
			if (arena->next) {
				arena->next->prev = arena;
			}
			FREE_ARENAS[index] = arena;
		}
		else if (arena->num_free + 1 == arena->total_count) {
			if (arena->prev) {
				arena->prev->next = arena->next;
			}
			else {
				FREE_ARENAS[index] = arena->next;
			}
			if (arena->next) {
				arena->next->prev = arena->prev;
			}
			sys_unmap(arena, ARENA_SIZE);
			return;
		}
		++arena->num_free;
		hdr->next = arena->freelist;
		arena->freelist = hdr;
	}
}
