#include "vm.h"
#include "arch/map.h"
#include "page.h"
#include "pmalloc.h"
#include "vmem.h"
#include "sched/task.h"

static VMem kernel_vmem = {};

void vm_kernel_init(usize base, usize size) {
	vmem_new(&kernel_vmem, "kernel vmem", (void*) base, size, PAGE_SIZE, NULL, NULL, NULL, 0, 0);
}

void* vm_kernel_alloc(usize count) {
	return vmem_alloc(&kernel_vmem, count * PAGE_SIZE, VM_INSTANTFIT);
}

void vm_kernel_dealloc(void* ptr, usize count) {
	vmem_free(&kernel_vmem, ptr, count * PAGE_SIZE);
}

void* vm_kernel_alloc_backed(usize count, PageFlags flags) {
	void* vm = vm_kernel_alloc(count);
	if (!vm) {
		return NULL;
	}

	Page* allocated_pages = NULL;
	for (usize i = 0; i < count; ++i) {
		Page* page = pmalloc(1);
		if (!page) {
			for (usize j = 0; j < i; ++j) {
				Page* next = allocated_pages->next;
				pfree(allocated_pages, 1);
				arch_unmap_page(KERNEL_MAP, (usize) vm + j * PAGE_SIZE, true);
				allocated_pages = next;
			}
			vm_kernel_dealloc(vm, count);
			return NULL;
		}
		page->next = allocated_pages;
		allocated_pages = page;

		arch_map_page(KERNEL_MAP, (usize) vm + i * PAGE_SIZE, page->phys, flags);
	}

	return vm;
}

void vm_kernel_dealloc_backed(void* ptr, usize count) {
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		usize phys = arch_virt_to_phys(KERNEL_MAP, virt);
		if (!phys) {
			panic("vm_kernel_dealloc_backed: phys is null\n");
		}
		arch_unmap_page(KERNEL_MAP, virt, true);

		pfree(page_from_addr(phys), 1);
	}
	vm_kernel_dealloc(ptr, count);
}

void vm_user_init(Task* task, usize base, usize size) {
	vmem_new(&task->user_vmem, "user vmem", (void*) base, size, PAGE_SIZE, NULL, NULL, NULL, 0, 0);
}

void vm_user_free(Task* task) {
	vmem_destroy(&task->user_vmem, false);
}

void* vm_user_alloc(Task* task, usize count) {
	return vmem_alloc(&task->user_vmem, count * PAGE_SIZE, VM_INSTANTFIT);
}
void vm_user_dealloc(Task* task, void* ptr, usize count) {
	vmem_free(&task->user_vmem, ptr, count * PAGE_SIZE);
}

void* vm_user_alloc_backed(Task* task, usize count, PageFlags flags, bool map_in_kernel) {
	void* vm = vm_user_alloc(task, count);
	if (!vm) {
		return NULL;
	}

	for (usize i = 0; i < count; ++i) {
		Page* page = pmalloc(1);
		if (!page) {
			for (usize j = 0; j < i; ++j) {
				Page* cur_page = task->allocated_pages;
				task_remove_page(task, cur_page);
				pfree(cur_page, 1);

				if (map_in_kernel) {
					arch_unmap_page(KERNEL_MAP, (usize) vm + j * PAGE_SIZE, true);
				}
				arch_user_unmap_page(task, (usize) vm + j * PAGE_SIZE, true);
			}
			vm_user_dealloc(task, vm, count);
			return NULL;
		}
		task_add_page(task, page);

		if (map_in_kernel) {
			arch_map_page(KERNEL_MAP, (usize) vm + i * PAGE_SIZE, page->phys, flags);
		}
		arch_user_map_page(task, (usize) vm + i * PAGE_SIZE, page->phys, flags);
	}

	return vm;
}

void vm_user_dealloc_kernel(void* ptr, usize count) {
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		arch_unmap_page(KERNEL_MAP, virt, true);
	}
}

void vm_user_dealloc_backed(Task* task, void* ptr, usize count, bool unmap_in_kernel) {
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		usize phys = arch_virt_to_phys(task->map, virt);
		if (!phys) {
			panic("vm_kernel_dealloc_backed: phys is null\n");
		}
		if (unmap_in_kernel) {
			arch_unmap_page(KERNEL_MAP, virt, true);
		}
		arch_user_unmap_page(task, virt, true);

		Page* page = page_from_addr(phys);
		task_remove_page(task, page);
		pfree(page, 1);
	}
	vm_user_dealloc(task, ptr, count);
}