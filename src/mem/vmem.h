#pragma once
#include <stddef.h>
#include <limits.h>

typedef enum {
	VM_INSTANTFIT = 0,
	VM_BESTFIT = 1 << 0,
	VM_NEXTFIT = 1 << 0 | 1 << 1,
	VM_NOSLEEP = 0 << 2,
	VM_SLEEP = 1 << 2
} VMFlags;

typedef enum {
	VMEM_SEG_FREE,
	VMEM_SEG_ALLOCATED,
	VMEM_SEG_SPAN
} VMemSegType;

typedef struct VMemSeg {
	struct VMemSeg* prev;
	struct VMemSeg* next;
	struct VMemSeg* seg_list_prev;
	struct VMemSeg* seg_list_next;
	void* base;
	size_t size;
	VMemSegType type;
} VMemSeg;

#define VMEM_FREELIST_COUNT (sizeof(void*) * CHAR_BIT)
#define VMEM_HASHTAB_COUNT 16

typedef struct VMem {
	const char* name;
	void* base;
	size_t size;
	size_t quantum;
	void* (*a_func)(struct VMem* self, size_t size, VMFlags flags);
	void* (*f_func)(struct VMem* self, void* ptr, size_t size);
	struct VMem* source;
	size_t qcache_max;
	VMemSeg* seg_list;
	VMemSeg* freelists[VMEM_FREELIST_COUNT];
	VMemSeg* hash_tab[VMEM_HASHTAB_COUNT];
	VMemSeg* free_segs;
	VMemSeg* seg_page_list;
	VMemSeg* last_alloc;
	VMemSeg static_segs[2];
} VMem;

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
		VMFlags flags);
void vmem_destroy(VMem* self);

void* vmem_alloc(VMem* self, size_t size, VMFlags flags);
void vmem_free(VMem* self, void* ptr, size_t size);
void* vmem_xalloc(
		VMem* self,
		size_t size,
		size_t align,
		size_t phase,
		size_t no_cross,
		void* min,
		void* max,
		VMFlags flags);
void vmem_xfree(VMem* self, void* ptr, size_t size);

void* vmem_add(VMem* self, void* base, size_t size, VMFlags flags);