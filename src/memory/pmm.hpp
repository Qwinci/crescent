#pragma once
#include "types.hpp"

struct Page {
	usize phys;
	Page* next;

	struct Mapping {
		Mapping* next;
		usize virt;
		PageMap* map;
	};
	Mapping* mappings;

	void add_mapping(PageMap* map, usize virt) {
		auto mapping = new Mapping {.next = mappings, .virt = virt, .map = map};
		mappings = mapping;
	}

	void remove_mapping(PageMap* map) {
		Mapping* prev = nullptr;
		for (auto mapping = mappings;; mapping = mapping->next) {
			if (mapping->map == map) {
				if (prev) {
					prev->next = mapping->next;
				}
				else {
					mappings = mapping->next;
				}
				ALLOCATOR.dealloc(mapping, sizeof(Mapping));
				break;
			}
			prev = mapping;
		}
	}

	enum class Queue : u8 {
		None,
		Free,
		Dma
	} queue;

	u8 reserved[39];
};

static_assert(sizeof(Page) == 64);

struct PRegion {
	PRegion* next;
	usize base;
	usize p_count;
	Page pages[];
};

#define ALIGNUP(Num, Align) (((Num) + (Align - 1)) & ~(Align - 1))
#define ALIGNDOWN(Num, Align) ((Num) & ~(Align - 1))

Page* phys_to_page(usize phys);