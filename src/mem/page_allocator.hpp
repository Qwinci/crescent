#pragma once
#include "types.hpp"
#include "page.hpp"

class PageAllocator {
public:
	/// Allocates count number of physically contiguous pages
	Page* alloc(usize count);
	/// Frees count number of pages previously allocated with alloc
	void free(Page* page, usize count);
	/// Adds memory to the page allocator
	/// @param phys Physical base
	/// @param virt Virtual base used to access phys
	/// @param size Size in bytes
	void add_mem(usize phys, void* virt, usize size);
private:
	static constexpr u8 FREELIST_COUNT = 11;
	Page* freelists[FREELIST_COUNT];

	void remove_first_entry(Page* entry);
	void remove_entry(Page* entry);
	void insert_entry(u8 list, Page* entry);
	void merge_entries(u8 list, Page* entry);
};

extern PageAllocator PAGE_ALLOCATOR;
