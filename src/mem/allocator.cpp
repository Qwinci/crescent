#include "allocator.hpp"
#include "algorithm.hpp"
#include "cstring.hpp"
#include "new.hpp"
#include "page_allocator.hpp"
#include "arch/map.hpp"

void* Allocator::alloc_indexed(u8 index, usize size) {
	if (size > 2048) {
		return nullptr;
	}

	auto list = freelists[index];
	if (list) {
		list->used += 1;
		auto node = list->freelist;
		list->freelist = node->next;
		if (!node->next) {
			freelists[index] = list->next;
			if (list->next) {
				list->next->prev = nullptr;
			}
		}
		return node;
	}

	Page* page = PAGE_ALLOCATOR.alloc(1);
	if (!page) {
		return nullptr;
	}

#ifdef CONFIG_HHDM
	void* virt = arch_to_virt(page->phys);
#else
#error Allocator doesn't support targets without hhdm
#endif
	memset(virt, 0, PAGE_SIZE);

	usize free_count = (PAGE_SIZE - sizeof(Header)) / size;
	auto header = new (virt) Header {
		.used = 1
	};

	usize free_start = reinterpret_cast<usize>(virt) + sizeof(Header);
	for (usize i = 1; i < free_count; ++i) {
		auto node = new (reinterpret_cast<void*>(free_start + i * size)) Node {
			.next = header->freelist
		};
		header->freelist = node;
	}

	freelists[index] = header;

	return reinterpret_cast<void*>(free_start);
}

void Allocator::free_indexed(void* ptr, u8 index) {
	auto header = reinterpret_cast<Header*>(ALIGNDOWN(reinterpret_cast<usize>(ptr), PAGE_SIZE));
	header->used -= 1;
	if (!header->freelist) {
		header->freelist = new (ptr) Node {};
		header->prev = nullptr;
		header->next = freelists[index];
		if (header->next) {
			header->next->prev = header;
		}
		freelists[index] = header;
	}
	else if (header->used == 0) {
		if (header->prev) {
			header->prev->next = header->next;
		}
		else {
			freelists[index] = header->next;
		}
		if (header->next) {
			header->next->prev = header->prev;
		}

#ifdef CONFIG_HHDM
		usize phys = arch_to_phys(header);
#else
#error Allocator doesn't support targets without hhdm
#endif
		PAGE_ALLOCATOR.free(Page::from_phys(phys), 1);
	}
}

Allocator ALLOCATOR;