#include "vmem.hpp"
#include "console.hpp"
#include "map.hpp"
#include "memory.hpp"
#include "pmm.hpp"
#include "tinyvmem/vmem.h"
#include "utils.hpp"

static Vmem kernel_vmem;
static Vmem user_vmem;

void vm_user_init(usize size) {
	vmem_init(
			&user_vmem, "User vmem", cast<void*>(0xFA000),
			size - 0xFA000, PAGE_SIZE, nullptr, nullptr, nullptr,
			0, VM_INSTANTFIT);
}

void* vm_user_alloc(usize count) {
	void* mem = vmem_alloc(&user_vmem, count * PAGE_SIZE, VM_INSTANTFIT);
	return mem;
}

void vm_user_dealloc(void* ptr, usize count) {
	vmem_free(&user_vmem, ptr, count * PAGE_SIZE);
}

void* vm_user_alloc_kernel_mapping(PageMap* map, void* ptr, usize count) {
	auto mapping = vm_kernel_alloc(count);

	for (usize i = 0; i < count; ++i) {
		auto phys = map->virt_to_phys(VirtAddr {ptr}.offset(i * 0x1000));
		if (!phys.as_usize()) {
			for (usize j = 0; j < i; ++j) {
				get_map()->unmap(VirtAddr {mapping}.offset(j * 0x1000), false, true);
				get_map()->refresh_page(VirtAddr {mapping}.offset(j * 0x1000).as_usize());
			}
			vm_kernel_dealloc(mapping, count);
			return nullptr;
		}
		get_map()->map(VirtAddr {mapping}.offset(i * 0x1000), phys, PageFlags::Rw);
		get_map()->refresh_page(VirtAddr {mapping}.offset(i * 0x1000).as_usize());
	}

	return mapping;
}

void vm_user_dealloc_kernel_mapping(void* ptr, usize count) {
	for (usize i = 0; i < count; ++i) {
		get_map()->unmap(VirtAddr {ptr}.offset(i * 0x1000), false, true);
	}

	vm_kernel_dealloc(ptr, count);
}

void* vm_user_alloc_backed(PageMap* map, usize count, AllocFlags flags) {
	void* mem = vm_user_alloc(count);
	if (!mem) {
		return nullptr;
	}

	struct Tmp {
		Tmp* next;
	};
	Tmp* tmp {};

	auto page_flags = PageFlags::User |
			(flags & AllocFlags::Rw ? PageFlags::Rw : PageFlags::Present) |
			(flags & AllocFlags::Exec ? PageFlags::Present : PageFlags::Nx);

	for (usize i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
		auto page = PAGE_ALLOCATOR.alloc_new();
		if (!page) {
			for (; tmp; tmp = tmp->next) {
				PAGE_ALLOCATOR.dealloc_new(cast<void*>(VirtAddr {tmp}.to_phys().as_usize()));
			}
			return nullptr;
		}

		auto t = cast<Tmp*>(PhysAddr {page}.to_virt().as_usize());
		t->next = tmp;
		tmp = t;

		map->map(VirtAddr {mem}.offset(i), PhysAddr {page}, page_flags);
		map->refresh_page(cast<usize>(mem) + i);
	}

	return mem;
}

void vm_user_dealloc_backed(PageMap* map, void* ptr, usize count) {
	for (usize i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
		auto phys = map->virt_to_phys(VirtAddr {ptr}.offset(i));
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(phys.as_usize()));
	}
	vm_user_dealloc(ptr, count);
}

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
		auto page = PAGE_ALLOCATOR.alloc_new(true, false);
		if (!page) {
			panic("failed to allocate physical memory in vm_kernel_alloc_backed");
		}
		auto page_entry = phys_to_page(cast<usize>(page));
		page_entry->add_mapping(get_map(), VirtAddr {mem}.offset(i).as_usize());
		get_map()->map(VirtAddr {mem}.offset(i), PhysAddr {page}, PageFlags::Rw);
		get_map()->refresh_page(cast<usize>(mem) + i);
	}
	return mem;
}

void vm_kernel_dealloc_backed(void* ptr, usize count) {
	for (usize i = 0; i < count * PAGE_SIZE; i += PAGE_SIZE) {
		auto phys = get_map()->virt_to_phys(VirtAddr {ptr}.offset(i));
		auto page = phys_to_page(phys.as_usize());
		page->remove_mapping(get_map());
		PAGE_ALLOCATOR.dealloc_new(cast<void*>(phys.as_usize()));
		get_map()->unmap(VirtAddr {ptr}.offset(i), false, true);
	}
	vm_kernel_dealloc(ptr, count);
}