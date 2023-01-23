#include "vmem.hpp"
#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "tinyvmem/vmem.h"
#include "utils.hpp"

static Vmem kernel_vmem;

void vm_kernel_init(usize base, usize size) {
	vmem_bootstrap();
	vmem_init(
			&kernel_vmem, "Kernel vmem", cast<void*>(base),
			size, PAGE_SIZE, nullptr, nullptr, nullptr,
			0, VM_INSTANTFIT);
}

void* vm_kernel_alloc(usize count) {
	void* mem = vmem_alloc(&kernel_vmem, count * PAGE_SIZE, VM_INSTANTFIT);
	if (!mem) {
		panic("vm_kernel_alloc failed");
	}
	return mem;
}

void vm_kernel_dealloc(void* ptr, usize count) {
	vmem_free(&kernel_vmem, ptr, count * PAGE_SIZE);
}

void* vm_kernel_alloc_backed(usize count) {
	void* mem = vm_kernel_alloc(count);
	for (usize i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
		auto page = PAGE_ALLOCATOR.alloc_new();
		if (!page) {
			panic("failed to allocate physical memory in vm_kernel_alloc_backed");
		}
		get_map()->map(VirtAddr {mem}.offset(i), PhysAddr {page}, PageFlags::Rw);
		get_map()->refresh_page(cast<usize>(mem) + i);
	}
	return mem;
}

void vm_kernel_dealloc_backed(void* ptr, usize count) {
	for (usize i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
		auto phys = get_map()->virt_to_phys(VirtAddr {ptr}.offset(i));
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(phys.as_usize()));
	}
	vm_kernel_dealloc(ptr, count);
}