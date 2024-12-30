#pragma once
#include "types.hpp"
#include "double_list.hpp"
#include "utils/spinlock.hpp"

class VMem {
public:
	void init(usize base, usize size, usize quantum);
	void destroy(bool assert_allocations);
	usize xalloc(usize size, usize min, usize max);
	void xfree(usize ptr, usize size);
private:
	static constexpr unsigned int size_to_index(usize size);

	static constexpr usize FREELIST_COUNT = sizeof(void*) * 8;
	static constexpr usize HASHTAB_COUNT = 16;

	struct Segment {
		DoubleListHook list_hook {};
		DoubleListHook seg_list_hook {};
		usize base {};
		usize size {};

		enum class Type {
			Free,
			Used,
			Span
		} type {};
	};
	DoubleList<Segment, &Segment::list_hook> freelists[FREELIST_COUNT] {};
	DoubleList<Segment, &Segment::seg_list_hook> seg_list {};
	DoubleList<Segment, &Segment::list_hook> hash_tab[HASHTAB_COUNT] {};
	Segment* free_segs {};
	Segment* seg_page_list {};
	usize _base {};
	usize _size {};
	usize _quantum {};
	Spinlock<void> lock {};

	Segment* seg_alloc();
	void seg_free(Segment* seg);
	void freelist_remove(Segment* seg);
	void freelist_insert_no_merge(Segment* seg);
	void freelist_insert(Segment* seg);
	void hashtab_insert(Segment* seg);
	Segment* hashtab_remove(usize key);
};
