#include "vmem.hpp"
#include "bit.hpp"
#include "new.hpp"

#ifdef TESTING
#include <cassert>
#include <cstdlib>

#define pmalloc(count) malloc(count * 0x1000)
#define pfree(ptr, count) free(reinterpret_cast<void*>(ptr))
template<typename T>
T* to_virt(void* ptr) {
	return static_cast<T*>(ptr);
}

template<typename T>
usize to_phys(T* ptr) {
	return reinterpret_cast<usize>(ptr);
}

#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))

#else
#include "mem.hpp"
#include "pmalloc.hpp"
#include "arch/paging.hpp"
#include "assert.hpp"
#endif

#include "algorithm.hpp"

#ifdef TESTING
#define PAGE_SIZE 0x1000
#endif

static void* alloc_page() {
	auto phys = pmalloc(1);
	if (!phys) {
		return nullptr;
	}
	return to_virt<void>(phys);
}

static void free_page(void* addr) {
	pfree(to_phys(addr), 1);
}

static u64 murmur64(u64 key) {
	key ^= key >> 33;
	key *= 0xFF51AFD7ED558CCDULL;
	key ^= key >> 33;
	key *= 0xC4CEB9FE1A85EC53ULL;
	key ^= key >> 33;
	return key;
}

constexpr unsigned int VMem::size_to_index(usize size) {
	return FREELIST_COUNT - kstd::countl_zero(size) - 1;
}

void VMem::init(usize base, usize size, usize quantum) {
	_base = base;
	_size = size;
	_quantum = quantum;

	auto* span = new (seg_alloc()) Segment {
		.base = base,
		.size = size,
		.type = Segment::Type::Span
	};

	auto* init = new (seg_alloc()) Segment {
		.base = base,
		.size = size,
		.type = Segment::Type::Free
	};

	seg_list.push(span);
	seg_list.push(init);

	freelists[size_to_index(size)].push(init);
}

void VMem::destroy(bool assert_allocations) {
	auto guard = lock.lock();

	while (seg_page_list) {
		auto* seg = seg_page_list;
		seg_page_list = static_cast<Segment*>(seg->list_hook.next);
		free_page(seg);
	}
	free_segs = nullptr;
	for (const auto& i : hash_tab) {
		if (assert_allocations) {
			assert(i.is_empty() && "tried to destroy vmem with allocations");
		}
	}
	seg_list.clear();
	for (auto& list : freelists) {
		list.clear();
	}
}

VMem::Segment* VMem::seg_alloc() {
	if (free_segs) {
		auto seg = free_segs;
		free_segs = static_cast<Segment*>(seg->list_hook.next);
		return seg;
	}
	else {
		auto* page = alloc_page();
		if (!page) {
			return nullptr;
		}

		auto* page_seg = new (page) Segment {
			.list_hook {
				.next = seg_page_list
			}
		};
		seg_page_list = page_seg;

		auto* seg = page_seg + 1;
		for (usize i = 1; i < PAGE_SIZE / sizeof(Segment); ++i) {
			seg->list_hook.next = free_segs;
			free_segs = seg;
			seg += 1;
		}

		seg = free_segs;
		free_segs = static_cast<Segment*>(seg->list_hook.next);
		return seg;
	}
}

void VMem::seg_free(VMem::Segment* seg) {
	seg->list_hook.next = free_segs;
	free_segs = seg;
}

usize VMem::xalloc(usize size, usize min, usize max) {
	size = ALIGNUP(size, _quantum);

	if (!max) {
		max = UINTPTR_MAX;
	}
	else if (max == min) {
		max = min + size;
	}

	auto index = size_to_index(size);

	if (!kstd::has_single_bit(size)) {
		index += 1;
	}

	auto guard = lock.lock();

	for (usize i = index; i < FREELIST_COUNT; ++i) {
		auto& list = freelists[i];
		auto* seg = list.pop_front();
		if (!seg) {
			continue;
		}

		if (seg->size > size) {
			usize start = kstd::max(seg->base, min);
			usize end = kstd::min(seg->base + seg->size, max);
			if (start > end || end - start < size) {
				list.push_front(seg);
				continue;
			}

			auto* alloc_seg = seg_alloc();
			if (!alloc_seg) {
				list.push_front(seg);
				return 0;
			}

			if (start != seg->base) {
				auto* new_seg = seg_alloc();
				if (!new_seg) {
					list.push_front(seg);
					seg_free(alloc_seg);
					return 0;
				}

				new (new_seg) Segment {
					.base = seg->base,
					.size = start - seg->base,
					.type = Segment::Type::Free
				};

				seg_list.insert_before(seg, new_seg);
				seg->base = start;
				seg->size -= new_seg->size;

				freelist_insert_no_merge(new_seg);
			}

			new (alloc_seg) Segment {
				.seg_list_hook {
					.prev = seg->seg_list_hook.prev,
					.next = seg
				},
				.base = seg->base,
				.size = size,
				.type = Segment::Type::Used
			};
			static_cast<Segment*>(seg->seg_list_hook.prev)->seg_list_hook.next = alloc_seg;
			seg->seg_list_hook.prev = alloc_seg;
			seg->base += size;
			seg->size -= size;

			freelist_insert(seg);
			hashtab_insert(alloc_seg);

			return alloc_seg->base;
		}
		else {
			hashtab_insert(seg);
			return seg->base;
		}
	}

	return 0;
}

void VMem::freelist_remove(VMem::Segment* seg) {
	usize index = size_to_index(seg->size);
	freelists[index].remove(seg);
}

void VMem::freelist_insert_no_merge(VMem::Segment* seg) {
	usize index = size_to_index(seg->size);
	auto& list = freelists[index];

	seg->type = Segment::Type::Free;
	list.push_front(seg);
}

void VMem::freelist_insert(VMem::Segment* seg) {
	while (true) {
		if (auto prev = static_cast<Segment*>(seg->seg_list_hook.prev);
			prev && prev->type == Segment::Type::Free &&
			prev->base + prev->size == seg->base) {
			if (auto next = static_cast<Segment*>(seg->seg_list_hook.next)) {
				next->seg_list_hook.prev = prev;
			}
			prev->seg_list_hook.next = seg->seg_list_hook.next;

			freelist_remove(prev);

			prev->size += seg->size;

			seg_free(seg);
			seg = prev;
			continue;
		}
		if (auto next = static_cast<Segment*>(seg->seg_list_hook.next);
			next && next->type == Segment::Type::Free &&
			seg->base + seg->size == next->base) {
			if (auto next_next = static_cast<Segment*>(next->seg_list_hook.next)) {
				next_next->seg_list_hook.prev = seg;
			}
			seg->seg_list_hook.next = next->seg_list_hook.next;

			freelist_remove(next);

			seg->size += next->size;

			seg_free(next);
			continue;
		}

		freelist_insert_no_merge(seg);
		break;
	}
}

void VMem::hashtab_insert(VMem::Segment* seg) {
	seg->type = Segment::Type::Used;

	u64 hash = murmur64(seg->base);
	hash_tab[hash % HASHTAB_COUNT].push_front(seg);
}

VMem::Segment* VMem::hashtab_remove(usize key) {
	u64 hash = murmur64(key);
	auto& list = hash_tab[hash % HASHTAB_COUNT];
	for (auto& entry : list) {
		if (entry.base == key) {
			list.remove(&entry);
			return &entry;
		}
	}
	return nullptr;
}

void VMem::xfree(usize ptr, usize size) {
	auto guard = lock.lock();

	auto* seg = hashtab_remove(ptr);
	if (!seg) {
		// todo
		return;
	}
	else if (seg->size != ALIGNUP(size, _quantum)) {
		assert(false && "tried to free with different size than allocated");
	}
	freelist_insert(seg);
}
