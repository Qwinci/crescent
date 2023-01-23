/* Implementation of the VMem resource allocator
as described in https://www.usenix.org/legacy/event/usenix01/full_papers/bonwick/bonwick.pdf
*/

#ifndef _VMEM_H
#define _VMEM_H
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Directs vmem to use the smallest
free segment that can satisfy the allocation. This
policy tends to minimize fragmentation of very
small, precious resources (cited from paper) */
#define VM_BESTFIT (1 << 0)

/* Directs vmem to provide a
good approximation to best−fit in guaranteed
constant time. This is the default allocation policy. (cited from paper) */
#define VM_INSTANTFIT (1 << 1)

/* Directs vmem to use the next free
segment after the one previously allocated. This is
useful for things like process IDs, where we want
to cycle through all the IDs before reusing them. (cited from paper) */
#define VM_NEXTFIT (1 << 2)

#define VM_SLEEP (1 << 3)
#define VM_NOSLEEP (1 << 4)

/* Used to eliminate cyclic dependencies when refilling the segment freelist:
We need to allocate new segments but to allocate new segments, we need to refill the list, this flag ensures that no refilling occurs. */
#define VM_BOOTSTRAP (1 << 5)

#define VMEM_ERR_NO_MEM 1

struct vmem;

/* Vmem allows one arena to import its resources from
another. vmem_create() specifies the source arena,
and the functions to allocate and free from that source. The arena imports new spans as needed, and gives
them back when all their segments have been freed. (cited from paper) These types describe those functions.
*/
typedef void *VmemAlloc(struct vmem *vmem, size_t size, int flags);
typedef void VmemFree(struct vmem *vmem, void *addr, size_t size);

/* We can't use boundary tags because the resource we're managing is not necessarily memory.
To counter this, we can use *external boundary tags*. For each segment in the arena
we allocate a boundary tag to manage it. */

/* sizeof(void *) * CHAR_BIT (8) freelists provides us with a freelist for every power-of-2 length that can fit within the host's virtual address space (64 bit) */
#define FREELISTS_N sizeof(void *) * CHAR_BIT
#define HASHTABLES_N 16

#define TAILQ_ENTRY(type) struct {type** prev; type* next;}
#define LIST_ENTRY(type) struct {type** prev; type* next;}
#define LIST_HEAD(head_name, type) struct head_name {type* root;}
#define TAILQ_HEAD(head_name, type) struct head_name {type* root; type** end;}

typedef struct vmem_segment
{
 enum
 {
	 SEGMENT_ALLOCATED,
	 SEGMENT_FREE,
	 SEGMENT_SPAN
 } type;

 bool imported; /* Non-zero if imported */

 uintptr_t base; /* base address of the segment */
 uintptr_t size; /* size of the segment */

 /* clang-format off */
TAILQ_ENTRY(vmem_segment) segqueue; /* Points to Vmem::segqueue */
LIST_ENTRY(vmem_segment) seglist; /* If free, points to Vmem::freelist, if allocated, points to Vmem::hashtable, else Vmem::spanlist */
 /* clang-format on */

} VmemSegment;

typedef LIST_HEAD(VmemSegList, vmem_segment) VmemSegList;
typedef TAILQ_HEAD(VmemSegQueue, vmem_segment) VmemSegQueue;

/* Statistics about a Vmem arena, NOTE: this isn't described in the original paper and was added by me. Inspired by Illumos and Solaris'vmem_kstat_t */
typedef struct
{
 size_t in_use; /* Memory in use */
 size_t import; /* Imported memory */
 size_t total;  /* Total memory in the area */
 size_t alloc;  /* Number of allocations */
 size_t free;   /* Number of frees */
} VmemStat;

/* Description of an arena, a collection of resources. An arena is simply a set of integers. */
typedef struct vmem
{
 char name[64];       /* Descriptive name for debugging purposes */
 void *base;          /* Start of initial span */
 size_t size;         /* Size of initial span */
 size_t quantum;      /* Unit of currency */
 VmemAlloc *alloc;    /* Import alloc function */
 VmemFree *free;      /* Import free function */
 struct vmem *source; /* Import arena */
 size_t qcache_max;   /* Maximum size to cache */
 int vmflag;          /* VM_SLEEP or VM_NOSLEEP */

 VmemSegQueue segqueue;
 VmemSegList freelist[FREELISTS_N];   /* Power of two freelists. Freelists[n] contains all free segments whose sizes are in the range [2^n, 2^n+1]  */
 VmemSegList hashtable[HASHTABLES_N]; /* Allocated segments */
 VmemSegList spanlist;                /* Span marker segments */

 VmemStat stat;
} Vmem;

/* Initializes a vmem arena (no malloc) */
int vmem_init(Vmem *vmem, const char *name, void *base, size_t size, size_t quantum, VmemAlloc *afunc, VmemFree *ffunc, Vmem *source, size_t qcache_max, int vmflag);

/* Destroys arena `vmp` */
void vmem_destroy(Vmem *vmp);

/* Allocates size bytes from vmp. Returns the allocated address on success, NULL on failure.
vmem_alloc() fails only if vmflag specifies VM_NOSLEEP and no resources are currently available.
vmflag may also specify an allocation policy (VM_BESTFIT, VM_INSTANTFIT, or VM_NEXTFIT).
If no policy is specified the default is VM_INSTANTFIT, which provides a good
approximation to best−fit in guaranteed constant time. (cited from paper) */
void *vmem_alloc(Vmem *vmp, size_t size, int vmflag);

/* Frees `size` bytes at address `addr` in arena `vmp` */
void vmem_free(Vmem *vmp, void *addr, size_t size);

/*
Allocates size bytes at offset phase from an align boundary such that the resulting segment
[addr, addr + size) is a subset of [minaddr, maxaddr) that does not straddle a nocross−
aligned boundary. vmflag is as above. One performance caveat: if either minaddr or maxaddr is
non−NULL, vmem may not be able to satisfy the allocation in constant time. If allocations within a
given [minaddr, maxaddr) range are common it is more efficient to declare that range to be its own
arena and use unconstrained allocations on the new arena (cited from paper).
*/
void *vmem_xalloc(Vmem *vmp, size_t size, size_t align, size_t phase,
			   size_t nocross, void *minaddr, void *maxaddr, int vmflag);

/*
Frees size bytes at addr, where addr was a constrained allocation. vmem_xfree() must be used if
the original allocation was a vmem_xalloc() because both routines bypass the quantum caches. (Cited from paper)
*/
void vmem_xfree(Vmem *vmp, void *addr, size_t size);

/* Adds the span [addr, addr + size) to arena vmp. Returns addr on success, NULL on failure.
vmem_add() will fail only if vmflag is VM_NOSLEEP and no resources are currently available. (cited from paper) */
void *vmem_add(Vmem *vmp, void *addr, size_t size, int vmflag);

/* Dumps the arena `vmp` using the `kprintf` function */
void vmem_dump(Vmem *vmp);

/* Initializes Vmem */
void vmem_bootstrap(void);

#endif