/*
* Public Domain implementation of the VMem Resource Allocator
*
* See: Adams, A. and Bonwick, J. (2001). Magazines and Vmem: Extending the Slab
* Allocator to Many CPUs and Arbitrary Resources.
* More implementation details are available in "vmem.h"
*/

#include "vmem.h"
#include "console.hpp"
#include "memory/map.hpp"
#include "memory/memory.hpp"

#define __KERNEL__

#ifndef __KERNEL__
#    include <assert.h>
#    include <stdio.h>
#    include <stdlib.h>
#    define vmem_printf printf
#    define ASSERT assert
#    define seg_alloc() malloc(sizeof(VmemSegment))
#    define seg_free(x) free(x)
#    define vmem_alloc_pages(x) malloc(x * 4096)
#endif

#define ARR_SIZE(x) (sizeof(x) / sizeof(*x))
#define VMEM_ADDR_MIN 0
#define VMEM_ADDR_MAX (~(uintptr_t)0)

/* Assuming FREELISTS_N is 64,
* we can calculate the freelist index by substracting the leading zero count from 64
* For example, the size 4096. clzl(4096) is 51, 64 - 51 is 13.
* We then need to substract 1 from 13 because 2^13 equals 8192.
*/
#define GET_LIST(size) (FREELISTS_N - __builtin_clzl(size) - 1)

#define VMEM_ALIGNUP(addr, align) \
   (((addr) + (align)-1) & ~((align)-1))

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

static Spinlock lock;

void* vmem_alloc_pages(size_t n) {
	if (n != 1) {
		panic("vmem_alloc_pages called with size other than 1");
	}
	auto mem = PAGE_ALLOCATOR.alloc_new();
	if (!mem) {
		panic("memory allocation failed in vmem_alloc_pages");
	}
	return PhysAddr {mem}.to_virt();
}

void vmem_lock() {
	lock.lock();
}

void vmem_unlock() {
	lock.unlock();
}

char* strcpy(char* dest, const char* src) {
	char* d = dest;
	while (*src) *dest++ = *src++;
	return d;
}

#define ASSERT(expr) ((expr) ? (void) 0 : panic(__FILE_NAME__, ":", __LINE__, ": assertion '", #expr, "' failed"))
#define vmem_printf(...)

#define LIST_HEAD_INITIALIZER(head) {}
#define LIST_EMPTY(head) (!(head)->root)
#define LIST_FIRST(head) ((head)->root)
#define LIST_REMOVE(elem, field) do { \
	if (auto next = (elem)->field.next) \
			next->field.prev = (elem)->field.prev; \
	*(elem)->field.prev = (elem)->field.next;\
} while (0)
#define LIST_INSERT_HEAD(head, elem, field) do { \
	if (((elem)->field.next = (head)->root) != nullptr) \
			(head)->root->field.prev = &(elem)->field.next;\
	(head)->root = (elem); \
	(elem)->field.prev = &(head)->root; \
} while (0)
#define LIST_INIT(head) do { \
	(head)->root = nullptr; \
} while (0)
#define LIST_FOREACH(var, head, field) \
for ((var) = ((head)->root); (var); (var) = ((var)->field.next))

#define TAILQ_FOREACH(var, head, field) \
for ((var) = ((head)->root); (var); (var) = ((var)->field.next))
#define TAILQ_INSERT_AFTER(head, list_elem, elem, field) do { \
	if (((elem)->field.next = (list_elem)->field.next) != nullptr) \
		(elem)->field.next->field.prev = &(elem)->field.next; \
	else \
		(head)->end = &(elem)->field.next; \
	(list_elem)->field.next = (elem); \
	(elem)->field.prev = &(list_elem)->field.next; \
} while (0)
#define TAILQ_INSERT_TAIL(head, elem, field) do { \
	(elem)->field.next = nullptr; \
	(elem)->field.prev = (head)->end; \
	*(head)->end = (elem); \
	(head)->end = &(elem)->field.next; \
} while (0)
#define TAILQ_INIT(head) do { \
	(head)->root = nullptr; \
	(head)->end = &(head)->root; \
} while (0)
#define TAILQ_PREV(elem, head_name, field) (*(((head_name*)(void*)((elem)->field.prev))->end))
#define TAILQ_NEXT(elem, field) ((elem)->field.next)
#define TAILQ_REMOVE(head, elem, field) do { \
	if (((elem)->field.next) != nullptr) \
		(elem)->field.next->field.prev = (elem)->field.prev; \
	else \
		(head)->end = (elem)->field.prev; \
	*(elem)->field.prev = (elem)->field.next; \
} while (0)
/* We need to keep a global freelist of segments because allocating virtual memory (e.g allocating a segment) requires segments to describe it. (kernel only)
In non-kernel code, this is handled by the host `malloc` and `free` standard library functions */
static VmemSegment static_segs[128];
static VmemSegList free_segs = LIST_HEAD_INITIALIZER(free_segs);
static int nfreesegs = 0;

static const char *seg_type_str[] = {
	   "allocated",
	   "free",
	   "span"};

#ifdef __KERNEL__

void vmem_lock(void);
void vmem_unlock(void);

static VmemSegment *seg_alloc(void)
{
   /* TODO: when bootstrapped, allocate boundary tags dynamically as described in the paper */
   VmemSegment *vsp;

   vmem_lock();
   ASSERT(!LIST_EMPTY(&free_segs));
   vsp = LIST_FIRST(&free_segs);
   LIST_REMOVE(vsp, seglist);
   nfreesegs--;
   vmem_unlock();

   return vsp;
}

static void seg_free(VmemSegment *seg)
{
   vmem_lock();
   LIST_INSERT_HEAD(&free_segs, seg, seglist);
   nfreesegs++;
   vmem_unlock();
}

#endif
static int repopulate_segments(void)
{
   struct
   {
	   VmemSegment segs[64];
   } * segblock;
   size_t i;

   if (nfreesegs >= 128)
	   return 0;

   /* Add 64 new segments */
   segblock = (decltype(segblock)) vmem_alloc_pages(1);

   for (i = 0; i < ARR_SIZE(segblock->segs); i++)
   {
	   seg_free(&segblock->segs[i]);
   }

   return 0;
}

static int seg_fit(VmemSegment *segment, size_t size, size_t align, size_t phase, size_t nocross, uintptr_t minaddr, uintptr_t maxaddr, uintptr_t *addrp)
{
   uintptr_t start, end;
   ASSERT(size > 0);
   ASSERT(segment->size >= size);

   start = MAX(segment->base, minaddr);
   end = MIN(segment->base + segment->size, maxaddr);

   if (start > end)
	   return -VMEM_ERR_NO_MEM;

   /*  Phase is the offset from the alignment boundary.
	*  For example, if `start` is 260, `phase` is 8 and align is `64`, we need to do the following calculation:
	*  ALIGN_UP(260 - 8, 64) = 256. 256 + 8 = 264. (264 % 64) is 8 as requested.
	*/
   start = VMEM_ALIGNUP(start - phase, align) + phase;

   /* If for some reason, start is smaller than the segment base, we need to ensure start is atleast as big as `align`
	* This can happen if, for example, we find that `start` is 0 and segment->base is 0x1000.
	* In this case, align is 0x1000. */
   if (start < segment->base)
   {
	   start += align;
   }

   ASSERT(nocross == 0 && "Not implemented (yet)");

   /* Ensure that `end` is bigger than `start` and we found a segment of the proper size */
   if (start <= end && (end - start) >= size)
   {
	   *addrp = start;
	   return 0;
   }

   return -VMEM_ERR_NO_MEM;
}

static uint64_t murmur64(uint64_t h)
{
   h ^= h >> 33;
   h *= 0xff51afd7ed558ccdL;
   h ^= h >> 33;
   h *= 0xc4ceb9fe1a85ec53L;
   h ^= h >> 33;
   return h;
}

static VmemSegList *hashtable_for_addr(Vmem *vmem, uintptr_t addr)
{
   /* Hash the address and get the remainder */
   uintptr_t idx = murmur64(addr) % ARR_SIZE(vmem->hashtable);
   return &vmem->hashtable[idx];
}

static void hashtab_insert(Vmem *vmem, VmemSegment *seg)
{
   LIST_INSERT_HEAD(hashtable_for_addr(vmem, seg->base), seg, seglist);
}

static VmemSegList *freelist_for_size(Vmem *vmem, size_t size)
{
   return &vmem->freelist[GET_LIST(size) - 1];
}

static int vmem_contains(Vmem *vmp, void *address, size_t size)
{
   VmemSegment *seg;
   uintptr_t start = (uintptr_t)address;
   uintptr_t end = start + size;

   TAILQ_FOREACH(seg, &vmp->segqueue, segqueue)
   {
	   if (start >= seg->base && end <= seg->base + seg->size)
	   {
		   return true;
	   }
   }
   return false;
}

static void vmem_add_to_freelist(Vmem *vm, VmemSegment *seg)
{
   LIST_INSERT_HEAD(freelist_for_size(vm, seg->size), seg, seglist);
}

static void vmem_insert_segment(Vmem *vm, VmemSegment *seg, VmemSegment *prev)
{

   TAILQ_INSERT_AFTER(&vm->segqueue, prev, seg, segqueue);
}

static VmemSegment *vmem_add_internal(Vmem *vmem, void *base, size_t size, bool import)
{
   VmemSegment *newspan, *newfree;

   newspan = seg_alloc();

   ASSERT(newspan);

   newspan->base = (uintptr_t)base;
   newspan->size = size;
   newspan->type = vmem_segment::SEGMENT_SPAN;
   newspan->imported = import;

   newfree = seg_alloc();

   ASSERT(newfree);

   newfree->base = (uintptr_t)base;
   newfree->size = size;
   newfree->type = vmem_segment::SEGMENT_FREE;

   TAILQ_INSERT_TAIL(&vmem->segqueue, newspan, segqueue);
   vmem_insert_segment(vmem, newfree, newspan);
   vmem_add_to_freelist(vmem, newfree);

   return newfree;
}

static int vmem_import(Vmem *vmp, size_t size, int vmflag)
{
   void *addr;
   VmemSegment *new_seg;
   if (!vmp->alloc)
	   return -VMEM_ERR_NO_MEM;

   addr = vmp->alloc(vmp->source, size, vmflag);

   if (!addr)
	   return -VMEM_ERR_NO_MEM;

   new_seg = vmem_add_internal(vmp, addr, size, true);

   if (!new_seg)
   {
	   vmp->free(vmp->source, addr, size);
   }

   return 0;
}

int vmem_init(Vmem *ret, const char *name, void *base, size_t size, size_t quantum, VmemAlloc *afunc, VmemFree *ffunc, Vmem *source, size_t qcache_max, int vmflag)
{
   size_t i;

   strcpy(ret->name, name);

   ret->base = base;
   ret->size = size;
   ret->quantum = quantum;
   ret->alloc = afunc;
   ret->free = ffunc;
   ret->source = source;
   ret->qcache_max = qcache_max;
   ret->vmflag = vmflag;
   ret->stat.free = size;
   ret->stat.total += size;
   ret->stat.in_use = 0;
   ret->stat.import = 0;

   LIST_INIT(&ret->spanlist);
   TAILQ_INIT(&ret->segqueue);

   for (i = 0; i < ARR_SIZE(ret->freelist); i++)
   {
	   LIST_INIT(&ret->freelist[i]);
   }

   for (i = 0; i < ARR_SIZE(ret->hashtable); i++)
   {
	   LIST_INIT(&ret->hashtable[i]);
   }

   /* Add initial span */
   if (!source && size)
	   vmem_add(ret, base, size, vmflag);

   return 0;
}

void vmem_destroy(Vmem *vmp)
{
   VmemSegment *seg;
   size_t i;

   for (i = 0; i < sizeof(vmp->hashtable) / sizeof(*vmp->hashtable); i++)
	   ASSERT(LIST_EMPTY(&vmp->hashtable[i]));

   TAILQ_FOREACH(seg, &vmp->segqueue, segqueue)
   {
	   seg_free(seg);
   }
}

void *vmem_add(Vmem *vmp, void *addr, size_t size, int vmflag)
{
   ASSERT(!vmem_contains(vmp, addr, size));

   vmp->stat.free += size;
   vmp->stat.total += size;
   (void)vmflag;
   return vmem_add_internal(vmp, addr, size, false);
}

void *vmem_xalloc(Vmem *vmp, size_t size, size_t align, size_t phase,
				 size_t nocross, void *minaddr, void *maxaddr, int vmflag)
{
   VmemSegList *first_list = freelist_for_size(vmp, size), *end = &vmp->freelist[FREELISTS_N], *list = NULL;
   VmemSegment *new_seg = NULL, *new_seg2 = NULL, *seg = NULL;
   uintptr_t start = 0;
   void *ret = NULL;

   ASSERT(nocross == 0 && "Not implemented yet");

   /* If we don't want a specific alignment, we can just use the quantum */
   /* FIXME: What if `align` is not quantum aligned? Maybe add an ASSERT() ? */

   if (align == 0)
   {
	   align = vmp->quantum;
   }

   if (!(vmflag & VM_BOOTSTRAP))
	   ASSERT(repopulate_segments() == 0);

   /* Allocate the new segments */
   /* NOTE: new_seg2 might be unused, in that case, it is freed */
   new_seg = seg_alloc();
   new_seg2 = seg_alloc();

   ASSERT(new_seg && new_seg2);

   while (true)
   {
	   if (vmflag & VM_INSTANTFIT) /* VM_INSTANTFIT */
	   {
		   /* We just get the first segment from the list. This ensures constant-time allocation.
			* Note that we do not need to check the size of the segments because they are guaranteed to be big enough (see freelist_for_size)
			*/
		   for (list = first_list; list < end; list++)
		   {
			   seg = LIST_FIRST(list);
			   if (seg != NULL)
			   {
				   if (seg_fit(seg, size, align, phase, nocross, (uintptr_t)minaddr, (uintptr_t)maxaddr, &start) == 0)
					   goto found;
			   }
		   }
	   }

	   else if (vmflag & VM_BESTFIT) /* VM_BESTFIT */
	   {
		   /* TODO: Should we bother going through the entire list to find the absolute best fit? */

		   /* We go through every segment in every list until we find the smallest free segment that can satisfy the allocation */
		   for (list = first_list; list < end; list++)
			   LIST_FOREACH(seg, list, seglist)
			   {
				   if (seg->size >= size)
				   {

					   /* Try to make the segment fit */
					   if (seg_fit(seg, size, align, phase, nocross, (uintptr_t)minaddr, (uintptr_t)maxaddr, &start) == 0)
						   goto found;
				   }
			   }
	   }
	   else if (vmflag & VM_NEXTFIT)
	   {
		   ASSERT(!"TODO: implement nextfit");
	   }

	   if (vmem_import(vmp, size, vmflag) == 0)
	   {
		   continue;
	   }

	   return NULL;
   }

found:
   ASSERT(seg != NULL);
   ASSERT(seg->type == vmem_segment::SEGMENT_FREE);
   ASSERT(seg->size >= size);

   /* Remove the segment from the freelist, it may be added back when modified */
   LIST_REMOVE(seg, seglist);

   if (seg->base != start)
   {
	   /* If the start is not the base of the segment, we need to create another segment;
		* new_seg2 is a free segment that starts at `base` and ends at `start-base`.
		* We also need to make make `seg` start at `start` and reduce its size.
		* For example, if we allocate a segment [0x100, 0x1000] in a [0, 0x10000] span, we need to split [0, 0x10000] into
		* [0x0, 0x100] (free), [0x100, 0x1000] (allocated), [0x1000, 0x10000] (free). In this case, `base` is 0 and `start` is 0x100.
		* This would create a segment with size 0x100-0 that starts at 0.
		*/
	   new_seg2->type = vmem_segment::SEGMENT_FREE;
	   new_seg2->base = seg->base;
	   new_seg2->size = start - seg->base;

	   /* Make `seg` start at `start`, following the example, this would make `(seg->base)` 0x100 */
	   seg->base = start;

	   /* Since we offset the segment by `start-(seg->base)`, we need to reduce `seg`'s size */
	   seg->size -= new_seg2->size;

	   vmem_add_to_freelist(vmp, new_seg2);

	   /* Put this new segment before the allocated segment */
	   vmem_insert_segment(vmp, new_seg2, TAILQ_PREV(seg, VmemSegQueue, segqueue));

	   /* Ensure it doesn't get freed */
	   new_seg2 = NULL;
   }

   ASSERT(seg->base == start);

   if (seg->size != size && (seg->size - size) > vmp->quantum - 1)
   {

	   /* In the case where the segment's size is bigger than the requested size, we need to split the segment into two:
		* one free part of size `seg->size - size` and another allocated one of size `size`. For example, if we want to allocate [0, 0x1000]
		* and the segment is [0, 0x10000], we have to create a new segment, [0, 0x1000] and offset the current segment by `size`. Therefore ending up with:
		*  [0, 0x1000] (allocated) [0x1000, 0x10000] */
	   new_seg->type = vmem_segment::SEGMENT_ALLOCATED;
	   new_seg->base = seg->base;
	   new_seg->size = size;

	   /* Offset the segment */
	   seg->base += size;
	   seg->size -= size;

	   /* Add it back to the freelist */
	   vmem_add_to_freelist(vmp, seg);

	   /* Put this new allocated segment before the segment */
	   vmem_insert_segment(vmp, new_seg, TAILQ_PREV(seg, VmemSegQueue, segqueue));

	   hashtab_insert(vmp, new_seg);
   }
   else
   {
	   seg->type = vmem_segment::SEGMENT_ALLOCATED;
	   hashtab_insert(vmp, seg);
	   seg_free(new_seg);
	   new_seg = seg;
   }

   if (new_seg2 != NULL)
	   seg_free(new_seg2);

   ASSERT(new_seg->size >= size);

   vmp->stat.free -= new_seg->size;
   vmp->stat.in_use += new_seg->size;

   new_seg->type = vmem_segment::SEGMENT_ALLOCATED;

   ret = (void *)new_seg->base;

   return ret;
}

void *vmem_alloc(Vmem *vmp, size_t size, int vmflag)
{
   return vmem_xalloc(vmp, size, 0, 0, 0, (void *)VMEM_ADDR_MIN, (void *)VMEM_ADDR_MAX, vmflag);
}

void vmem_xfree(Vmem *vmp, void *addr, size_t size)
{
   VmemSegment *seg, *neighbor;
   VmemSegList *list;

   list = hashtable_for_addr(vmp, (uintptr_t)addr);

   LIST_FOREACH(seg, list, seglist)
   {
	   if (seg->base == (uintptr_t)addr)
	   {
		   break;
	   }
   }

   ASSERT(seg->size == size);

   /* Remove the segment from the hashtable */
   LIST_REMOVE(seg, seglist);

   /* Coalesce to the right */
   neighbor = TAILQ_NEXT(seg, segqueue);

   if (neighbor && neighbor->type == vmem_segment::SEGMENT_FREE)
   {
	   /* Remove our neighbor since we're merging with it */
	   LIST_REMOVE(neighbor, seglist);

	   TAILQ_REMOVE(&vmp->segqueue, neighbor, segqueue);

	   seg->size += neighbor->size;

	   seg_free(neighbor);
   }

   /* Coalesce to the left */
   neighbor = TAILQ_PREV(seg, VmemSegQueue, segqueue);

   if (neighbor->type == vmem_segment::SEGMENT_FREE)
   {
	   LIST_REMOVE(neighbor, seglist);
	   TAILQ_REMOVE(&vmp->segqueue, neighbor, segqueue);

	   seg->size += neighbor->size;
	   seg->base = neighbor->base;

	   seg_free(neighbor);
   }

   neighbor = TAILQ_PREV(seg, VmemSegQueue, segqueue);

   ASSERT(neighbor->type == vmem_segment::SEGMENT_SPAN || neighbor->type == vmem_segment::SEGMENT_ALLOCATED);

   seg->type = vmem_segment::SEGMENT_FREE;

   if (vmp->free != NULL && neighbor->type == vmem_segment::SEGMENT_SPAN && neighbor->imported == true && neighbor->size == seg->size)
   {
	   uintptr_t span_addr = seg->base;
	   size_t span_size = seg->size;

	   TAILQ_REMOVE(&vmp->segqueue, seg, segqueue);
	   seg_free(seg);
	   TAILQ_REMOVE(&vmp->segqueue, neighbor, segqueue);
	   seg_free(neighbor);

	   vmp->free(vmp->source, (void *)span_addr, span_size);
   }
   else
   {
	   vmem_add_to_freelist(vmp, seg);
   }

   vmp->stat.in_use -= size;
   vmp->stat.free += size;
}

void vmem_free(Vmem *vmp, void *addr, size_t size)
{
   vmem_xfree(vmp, addr, size);
}

void vmem_dump(Vmem *vmp)
{
   VmemSegment *span;
   size_t i;

   vmem_printf("-- VMem arena \"%s\" segments -- \n", vmp->name);

   TAILQ_FOREACH(span, &vmp->segqueue, segqueue)
   {
	   vmem_printf("[0x%lx, 0x%lx] (%s)",
				   span->base, span->base + span->size, seg_type_str[span->type]);
	   if (span->imported)
		   vmem_printf("(imported)");
	   vmem_printf("\n");
   }

   vmem_printf("Hashtable:\n ");

   for (i = 0; i < ARR_SIZE(vmp->hashtable); i++)
	   LIST_FOREACH(span, &vmp->hashtable[i], seglist)
	   {
		   vmem_printf("%lx: [address: %p, size %p]\n", murmur64(span->base), (void *)span->base, (void *)span->size);
	   }
   vmem_printf("Stat:\n");
   vmem_printf("- in_use: %ld\n", vmp->stat.in_use);
   vmem_printf("- free: %ld\n", vmp->stat.free);
   vmem_printf("- total: %ld\n", vmp->stat.total);
}

void vmem_bootstrap(void)
{
   size_t i;
   for (i = 0; i < ARR_SIZE(static_segs); i++)
   {
	   seg_free(&static_segs[i]);
   }
}