#pragma once
#include "types.hpp"

constexpr usize PAGE_SIZE = 0x1000;

class PageAllocator {
public:
	void* alloc_new();
	void* alloc_low_new(usize count);
	void dealloc_new(void* ptr);
	void dealloc_low_new(void* ptr, usize count);
	void add_mem(usize base, usize size);
	void init_bitmap(usize base, usize max_phys);
	static constexpr usize get_bitmap_real_size(usize max_addr) {
		auto count = max_addr / PAGE_SIZE / 8;
		return count % 8 == 0 ? count : count + 8 - (count % 8);
	}
	static constexpr usize get_bitmap_size(usize max_addr) {
		return max_addr / PAGE_SIZE / 8;
	}
private:
	class Bitmap {
		u64* data;
		usize count;
	public:
		void init(u64* ptr, usize size) {
			data = ptr;
			count = size;
			for (usize i = 0; i < size / 64; ++i) {
				data[i] = UINT64_MAX;
			}
		}

		class reference {
		public:
			constexpr ~reference() = default;
			constexpr reference& operator=(bool x) noexcept {
				*ptr &= ~(1ULL << index);
				*ptr |= as<u64>(x) << index;
				return *this;
			}

			constexpr operator bool() const noexcept { // NOLINT(google-explicit-constructor)
				return *ptr & (1ULL << index);
			}
		private:
			constexpr reference(u64* ptr, u8 index) : ptr {ptr}, index {index} {}

			u64* ptr;
			u8 index;
			friend class Bitmap;
		};

		constexpr bool operator[](size_t pos) const {
			return data[pos / 64] & (1ULL << (pos % 64));
		}

		constexpr reference operator[](size_t pos) {
			return reference {data + pos / 64, as<u8>(pos % 64)};
		}

		[[nodiscard]] constexpr usize size() const {
			return count;
		}
	};

	Bitmap map {};

	struct Node {
		Node* next;
	};

	Node* root {};
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