#include "malloc.hpp"
#include "bit.hpp"
#include "pmalloc.hpp"
#include "mem.hpp"
#include "new.hpp"
#include "arch/paging.hpp"
#include "vspace.hpp"
#include "sched/mutex.hpp"

constexpr usize Allocator::size_to_index(usize size) {
	if (size <= 16) {
		return 0;
	}

	return sizeof(usize) * 8 - kstd::countl_zero(size - 1) - 4;
}

constexpr usize index_to_size(usize index) {
	return 16ULL << index;
}

static Mutex<void> FREELIST_MUTEX[Allocator::FREELIST_COUNT] {};

#if ARCH_USER
#include <stdlib.h>
#endif

void* Allocator::alloc(usize size) {
	if (size == PAGE_SIZE) {
		auto page = pmalloc(1);
		if (!page) {
			return nullptr;
		}
		return to_virt<void>(page);
	}
	else if (size > 2048) {
#if ARCH_USER
		return malloc(size);
#else
		return KERNEL_VSPACE.alloc_backed(size, PageFlags::Read | PageFlags::Write);
#endif
	}

	auto index = size_to_index(size);
	assert(index < sizeof(freelists) / sizeof(*freelists));

	auto guard = FREELIST_MUTEX[index].lock();

	auto& list = freelists[index];
	if (!list.is_empty()) {
		auto& front = *list.begin();
		assert(!front.list.is_empty());

		auto ptr = front.list.pop();
		++front.count;
		if (front.count == front.max) {
			list.remove(&front);
		}
		assert(ptr);
		return ptr;
	}

	auto page = pmalloc(1);
	if (!page) {
		return nullptr;
	}

	auto* hdr = new (to_virt<void>(page)) Header {};

	auto chunk_size = index_to_size(index);
	assert(chunk_size >= size);

	hdr->count = 1;
	hdr->max = (PAGE_SIZE - sizeof(Header)) / chunk_size;
	for (usize i = 1; i < hdr->max; ++i) {
		auto* node = new (offset(&hdr[1], Node*, i * chunk_size)) Node {};
		hdr->list.push(node);
	}

	if (hdr->count != hdr->max) {
		list.push(hdr);
	}

	return &hdr[1];
}

void Allocator::free(void* ptr, usize size) {
	if (!ptr || !size) {
		return;
	}

	if (size == PAGE_SIZE) {
		pfree(to_phys(ptr), 1);
		return;
	}
	else if (size > 2048) {
#if ARCH_USER
		::free(ptr);
#else
		KERNEL_VSPACE.free_backed(ptr, size);
#endif
		return;
	}

	auto index = size_to_index(size);
	assert(index < sizeof(freelists) / sizeof(*freelists));
	assert(index_to_size(index) >= size);

	auto guard = FREELIST_MUTEX[index].lock();

	auto* hdr = reinterpret_cast<Header*>(ALIGNDOWN(reinterpret_cast<usize>(ptr), PAGE_SIZE));
	if (hdr->count == hdr->max) {
		freelists[index].push(hdr);
	}
	else if (hdr->count == 1) {
		freelists[index].remove(hdr);
		pfree(to_phys(hdr), 1);
		return;
	}

	--hdr->count;

	auto* node = new (ptr) Node {};
	hdr->list.push(node);
}

Allocator ALLOCATOR;
