#include "vmem.h"
#include "assert.h"
#include "pmalloc.h"
#include "utils.h"
#include "utils/math.h"
#include <stdint.h>

#define PAGE_SIZE 0x1000

#ifdef USERSPACE
#include <assert.h>
#include <stdlib.h>
#define ASSERT(expr) assert(expr)
#define ALLOC_PAGE malloc(PAGE_SIZE)
#define FREE_PAGE(ptr) free(ptr)
#endif

static void* alloc_page_wrapper() {
	Page* page = pmalloc(1);
	if (!page) {
		return NULL;
	}
	return to_virt(page->phys);
}

#define ASSERT(expr) assert(expr)
#define ALLOC_PAGE alloc_page_wrapper()
#define FREE_PAGE(ptr) pfree(page_from_addr(to_phys(ptr)), 1)

#define LIST_INDEX_FOR_SIZE(size) (VMEM_FREELIST_COUNT - __builtin_clzll(size) - 1)
#define ALIGNUP(value, align) (((value) + ((align) - 1)) & ~((align) - 1))

static VMemSeg* seg_alloc(VMem* self) {
	if (self->free_segs) {
		VMemSeg* seg = self->free_segs;
		self->free_segs = seg->next;
		return seg;
	}
	else {
		void* page = ALLOC_PAGE;
		if (!page) {
			return NULL;
		}

		VMemSeg* page_seg = (VMemSeg*) page;
		page_seg->next = self->seg_page_list;
		self->seg_page_list = page_seg;

		VMemSeg* seg = page_seg + 1;

		for (size_t i = 1; i < PAGE_SIZE / sizeof(VMemSeg); ++i) {
			seg->next = self->free_segs;
			self->free_segs = seg;
			seg += 1;
		}

		seg = self->free_segs;
		self->free_segs = seg->next;
		return seg;
	}
}

static void freelist_remove(VMem* self, VMemSeg* seg) {
	size_t index = LIST_INDEX_FOR_SIZE(seg->size);

	if (seg->prev) {
		seg->prev->next = seg->next;
	}
	else {
		self->freelists[index] = seg->next;
	}
	if (seg->next) {
		seg->next->prev = seg->prev;
	}
}

static void freelist_insert(VMem* self, VMemSeg* seg) { // NOLINT(misc-no-recursion)
	if (seg->seg_list_prev && seg->seg_list_prev->type == VMEM_SEG_FREE &&
			(size_t) seg->seg_list_prev->base + seg->seg_list_prev->size == (size_t) seg->base) {
		VMemSeg* prev = seg->seg_list_prev;
		if (seg->seg_list_next) {
			seg->seg_list_next->seg_list_prev = prev;
		}
		prev->seg_list_next = seg->seg_list_next;

		freelist_remove(self, prev);

		prev->size += seg->size;

		seg->next = self->free_segs;
		self->free_segs = seg;

		seg = prev;
	}
	if (seg->seg_list_next && seg->seg_list_next->type == VMEM_SEG_FREE &&
			(size_t) seg->base + seg->size == (size_t) seg->seg_list_next->base) {
		VMemSeg* next = seg->seg_list_next;
		if (next->seg_list_next) {
			next->seg_list_next->seg_list_prev = seg;
		}
		seg->seg_list_next = next->seg_list_next;

		freelist_remove(self, next);

		seg->size += next->size;

		next->next = self->free_segs;
		self->free_segs = next;
	}

	size_t list_index = LIST_INDEX_FOR_SIZE(seg->size);
	VMemSeg* list = self->freelists[list_index];

	seg->prev = NULL;
	seg->type = VMEM_SEG_FREE;
	if (!list) {
		seg->next = NULL;
	}
	else {
		seg->next = list;
		list->prev = seg;
	}
	self->freelists[list_index] = seg;
}

static void hashtab_insert(VMem* self, VMemSeg* seg) {
	uint64_t hash = murmur64((uint64_t) seg->base);
	VMemSeg* hashtab = self->hash_tab[hash % VMEM_HASHTAB_COUNT];
	seg->next = NULL;
	seg->type = VMEM_SEG_ALLOCATED;
	if (!hashtab) {
		self->hash_tab[hash % VMEM_HASHTAB_COUNT] = seg;
		seg->prev = NULL;
	}
	else {
		while (hashtab->next) {
			hashtab = hashtab->next;
		}
		hashtab->next = seg;
		seg->prev = hashtab;
	}
}

static VMemSeg* hashtab_remove(VMem* self, void* ptr) {
	uint64_t hash = murmur64((uint64_t) ptr);
	VMemSeg* hashtab = self->hash_tab[hash % VMEM_HASHTAB_COUNT];
	while (hashtab) {
		if (hashtab->base == ptr) {
			if (hashtab->prev) {
				hashtab->prev->next = hashtab->next;
			}
			else {
				self->hash_tab[hash % VMEM_HASHTAB_COUNT] = hashtab->next;
			}
			if (hashtab->next) {
				hashtab->next->prev = hashtab->prev;
			}
			return hashtab;
		}
		hashtab = hashtab->next;
	}
	return NULL;
}

void vmem_new(
		VMem* self,
		const char* name,
		void* base,
		size_t size,
		size_t quantum,
		void* (*a_func)(VMem* self, size_t size, VMFlags flags),
		void* (*f_func)(VMem* self, void* ptr, size_t size),
		VMem* source,
		size_t qcache_max,
		VMFlags flags) {
	*self = (VMem) {
		.name = name,
		.base = base,
		.size = size,
		.quantum = quantum,
		.a_func = a_func,
		.f_func = f_func,
		.source = source,
		.qcache_max = qcache_max
	};
	ASSERT(!a_func && !f_func && "importing is not implemented");

	VMemSeg* init = &self->static_segs[1];
	init->type = VMEM_SEG_FREE;
	init->base = base;
	init->size = size;
	init->seg_list_next = NULL;
	init->next = NULL;

	VMemSeg* span = &self->static_segs[0];
	span->type = VMEM_SEG_SPAN;
	span->base = base;
	span->size = size;
	span->prev = NULL;
	span->next = NULL;
	span->seg_list_next = init;
	self->seg_list = span;

	init->seg_list_prev = span;

	self->seg_list = span;

	size_t list_index = LIST_INDEX_FOR_SIZE(size);

	self->freelists[list_index] = init;
}

void vmem_destroy(VMem* self, bool assert_allocations) {
	while (self->seg_page_list) {
		VMemSeg* seg = self->seg_page_list;
		self->seg_page_list = seg->next;
		FREE_PAGE((void*) seg);
	}
	self->free_segs = NULL;
	for (size_t i = 0; i < VMEM_HASHTAB_COUNT; ++i) {
		if (assert_allocations) {
			ASSERT(self->hash_tab[i] && "tried to destroy vmem with allocations");
		}
		self->hash_tab[i] = NULL;
	}
}

void* vmem_alloc(VMem* self, size_t size, VMFlags flags) {
	return vmem_xalloc(self, size, 0, 0, 0, NULL, NULL, flags);
}

void vmem_free(VMem* self, void* ptr, size_t size) {
	vmem_xfree(self, ptr, size);
}

static void split_seg(VMemSeg* seg, VMemSeg* alloc_seg, size_t size) {
	alloc_seg->base = seg->base;
	alloc_seg->size = size;
	alloc_seg->seg_list_prev = seg->seg_list_prev;
	seg->seg_list_prev->seg_list_next = alloc_seg;
	alloc_seg->type = VMEM_SEG_ALLOCATED;
	alloc_seg->seg_list_next = seg;

	seg->seg_list_prev = alloc_seg;
	seg->base = (void*) ((uintptr_t) seg->base + size);
	seg->size -= size;
}

static void seg_list_insert_after(VMemSeg** prev, VMemSeg* seg) {
	if (*prev) {
		VMemSeg* p = *prev;
		seg->seg_list_prev = p;
		seg->seg_list_next = p->seg_list_next;
		if (p->seg_list_next) {
			p->seg_list_next->seg_list_prev = seg;
		}
		p->seg_list_next = seg;
	}
	else {
		*prev = seg;
		seg->seg_list_prev = NULL;
		seg->seg_list_next = NULL;
	}
}

void* vmem_xalloc( // NOLINT(misc-no-recursion)
		VMem* self,
		size_t size,
		size_t align,
		size_t phase,
		size_t no_cross,
		void* min,
		void* max,
		VMFlags flags) {
	assert(self);
	size = ALIGNUP(size, self->quantum);

	ASSERT(phase == 0 && "phase is not implemented");
	ASSERT(align == 0 && "align is not implemented");
	ASSERT(no_cross == 0 && "no_cross is not implemented");
	ASSERT(min == NULL && "min is not implemented");
	ASSERT(max == NULL && "max is not implemented");

	size_t list_index = LIST_INDEX_FOR_SIZE(size);

	if ((flags & 0b11) == VM_INSTANTFIT) {
		if (size & (size - 1)) {
			list_index += 1;
		}

		for (size_t i = list_index; i < VMEM_FREELIST_COUNT; ++i) {
			VMemSeg* list = self->freelists[i];
			if (!list) {
				continue;
			}

			self->freelists[i] = list->next;
			if (list->next) {
				list->next->prev = NULL;
			}

			if (list->size > size) {
				VMemSeg* alloc_seg = seg_alloc(self);
				if (!alloc_seg) {
					self->freelists[i] = list;
					if (list->next) {
						list->next->prev = list;
					}
					return NULL;
				}

				split_seg(list, alloc_seg, size);

				freelist_insert(self, list);
				hashtab_insert(self, alloc_seg);

				self->last_alloc = alloc_seg;

				return alloc_seg->base;
			}
			else {
				hashtab_insert(self, list);
				self->last_alloc = list;
				return list->base;
			}
		}
	}
	else if ((flags & 0b11) == VM_BESTFIT) {
		for (size_t i = list_index; i < VMEM_FREELIST_COUNT; ++i) {
			VMemSeg* list = self->freelists[i];

			if (!list) {
				continue;
			}

			size_t best_remaining = SIZE_MAX;
			while (list) {
				if (list->size >= size) {
					if (list->size - size < best_remaining) {
						if (list->size - size == 0) {
							break;
						}
						best_remaining = list->size - size;
					}
				}
				if (!list->next) {
					break;
				}
				list = list->next;
			}

			if (list->prev) {
				list->prev->next = list->next;
			}
			else {
				self->freelists[i] = list->next;
			}
			if (list->next) {
				list->next->prev = NULL;
			}

			if (list->size > size) {
				VMemSeg* alloc_seg = seg_alloc(self);
				if (!alloc_seg) {
					if (list->prev) {
						list->prev->next = list;
					}
					else {
						self->freelists[i] = list;
					}
					if (list->next) {
						list->next->prev = list;
					}
					return NULL;
				}

				alloc_seg->base = list->base;
				alloc_seg->size = size;
				alloc_seg->seg_list_prev = list->seg_list_prev;
				alloc_seg->type = VMEM_SEG_ALLOCATED;
				if (list->seg_list_prev) {
					list->seg_list_prev->seg_list_next = alloc_seg;
				}
				alloc_seg->seg_list_next = list;

				list->size -= size;
				list->base = (void*) ((uintptr_t) list->base + size);
				list->seg_list_prev = alloc_seg;

				freelist_insert(self, list);
				hashtab_insert(self, alloc_seg);

				self->last_alloc = alloc_seg;
				return alloc_seg->base;
			}
			else {
				hashtab_insert(self, list);
				self->last_alloc = list;
				return list->base;
			}
		}
	}
	else if ((flags & 0b11) == VM_NEXTFIT) {
		VMemSeg* seg = self->last_alloc;

		if (!seg) {
			return vmem_xalloc(self, size, 0, 0, 0, NULL, NULL, flags & ~(0b11));
		}

		while (seg->seg_list_next) {
			seg = seg->seg_list_next;
			if (seg->type == VMEM_SEG_FREE) {
				if (seg->prev) {
					seg->prev->next = seg->next;
				}
				else {
					self->freelists[list_index] = seg->next;
				}
				if (seg->next) {
					seg->next->prev = seg->prev;
				}

				if (seg->size > size) {
					VMemSeg* alloc_seg = seg_alloc(self);
					if (!alloc_seg) {
						if (seg->prev) {
							seg->prev->next = seg;
						}
						else {
							self->freelists[list_index] = seg;
						}
						if (seg->next) {
							seg->next->prev = seg;
						}
						return NULL;
					}

					split_seg(seg, alloc_seg, size);

					freelist_insert(self, seg);
					hashtab_insert(self, alloc_seg);

					self->last_alloc = alloc_seg;
					return alloc_seg->base;
				}
				else {
					hashtab_insert(self, seg);
					self->last_alloc = seg;
					return seg->base;
				}
			}
		}
	}

	return NULL;
}

void vmem_xfree(VMem* self, void* ptr, size_t size) {
	VMemSeg* seg = hashtab_remove(self, ptr);
	if (!seg) {
		// todo
		//ASSERT(false && "tried to free not allocated pointer");
		return;
	}
	else if (seg->size != ALIGNUP(size, self->quantum)) {
		ASSERT(false && "tried to free a pointer with different size than allocated");
	}
	freelist_insert(self, seg);
}

void* vmem_add(VMem* self, void* base, size_t size, VMFlags flags) {
	VMemSeg* seg = seg_alloc(self);
	if (!seg) {
		return NULL;
	}

	seg->type = VMEM_SEG_FREE;
	seg->base = base;
	seg->size = size;
	seg->seg_list_next = NULL;
	seg->next = NULL;

	VMemSeg* span = seg_alloc(self);
	if (!span) {
		return NULL;
	}

	span->type = VMEM_SEG_SPAN;
	span->base = base;
	span->size = size;
	span->prev = NULL;
	span->next = NULL;
	span->seg_list_next = seg;

	seg->seg_list_prev = span;

	if (base < self->seg_list->base) {
		self->seg_list->seg_list_prev = seg;
		seg->seg_list_next = self->seg_list;
		self->seg_list = span;
	}
	else {
		VMemSeg* list = self->seg_list;
		while (true) {
			if (!list->seg_list_next || base < list->seg_list_next->base) {
				seg->seg_list_next = list->seg_list_next;
				if (list->seg_list_next) {
					list->seg_list_next->seg_list_prev = seg;
				}
				list->seg_list_next = span;
				span->seg_list_prev = list;
				break;
			}
			list = list->seg_list_next;
		}
	}

	freelist_insert(self, seg);

	return base;
}