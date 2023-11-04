#include "vm.h"
#include "arch/map.h"
#include "page.h"
#include "pmalloc.h"
#include "sched/mutex.h"
#include "sched/process.h"
#include "vmem.h"
#include "string.h"
#include "utils.h"

static VMem kernel_vmem = {};
static Mutex KERNEL_VM_LOCK = {};

void vm_kernel_init(usize base, usize size) {
	vmem_new(&kernel_vmem, "kernel vmem", (void*) base, size, PAGE_SIZE, NULL, NULL, NULL, 0, 0);
}

void* vm_kernel_alloc(usize count) {
	mutex_lock(&KERNEL_VM_LOCK);
	void* ptr = vmem_alloc(&kernel_vmem, count * PAGE_SIZE, VM_INSTANTFIT);
	mutex_unlock(&KERNEL_VM_LOCK);
	return ptr;
}

void vm_kernel_dealloc(void* ptr, usize count) {
	mutex_lock(&KERNEL_VM_LOCK);
	vmem_free(&kernel_vmem, ptr, count * PAGE_SIZE);
	mutex_unlock(&KERNEL_VM_LOCK);
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

void vm_user_init(Process* process, usize base, usize size) {
	usize low_base = base;
	usize half_size = ALIGNDOWN(size / 2, PAGE_SIZE);
	usize high_base = base + half_size;
	vmem_new(&process->low_vmem, "user low vmem", (void*) low_base, half_size, PAGE_SIZE, NULL, NULL, NULL, 0, 0);
	vmem_new(&process->high_vmem, "user high vmem", (void*) high_base, half_size, PAGE_SIZE, NULL, NULL, NULL, 0, 0);
}

void vm_user_free(Process* process) {
	vmem_destroy(&process->low_vmem, false);
	vmem_destroy(&process->high_vmem, false);
}

void* vm_user_alloc(Process* process, void* at, usize count, bool high) {
	mutex_lock(&process->vmem_lock);
	void* res = vmem_xalloc(high ? &process->high_vmem : &process->low_vmem, count * PAGE_SIZE, 0, 0, 0, at, at, VM_INSTANTFIT);
	mutex_unlock(&process->vmem_lock);
	return res;
}

void vm_user_dealloc(Process* process, void* ptr, usize count, bool high) {
	mutex_lock(&process->vmem_lock);
	vmem_free(high ? &process->high_vmem : &process->low_vmem, ptr, count * PAGE_SIZE);
	mutex_unlock(&process->vmem_lock);
}

void* vm_user_alloc_backed(Process* process, void* at, usize count, PageFlags flags, bool high, void** kernel_mapping) {
	void* vm = vm_user_alloc(process, at, count, high);
	if (!vm) {
		return NULL;
	}
	void* kernel_vm = NULL;
	if (kernel_mapping) {
		*kernel_mapping = vm_kernel_alloc(count);
		if (!*kernel_mapping) {
			vm_user_dealloc(process, vm, count, high);
			return NULL;
		}
		kernel_vm = *kernel_mapping;
	}

	mutex_lock(&process->mapping_lock);
	if (!process_add_mapping(process, (usize) vm, count * PAGE_SIZE, (flags & PF_WRITE) ? MAPPING_FLAG_W : 0)) {
		mutex_unlock(&process->mapping_lock);
		vm_user_dealloc(process, vm, count, high);
		if (kernel_vm) {
			vm_kernel_dealloc(kernel_vm, count);
		}
		return NULL;
	}
	mutex_unlock(&process->mapping_lock);
	for (usize i = 0; i < count; ++i) {
		Page* page = pmalloc(1);
		if (!page) {
			for (usize j = 0; j < i; ++j) {
				Page* p = page_from_addr(arch_virt_to_phys(process->map, (usize) vm + j * PAGE_SIZE));
				pfree(p, 1);

				if (kernel_vm) {
					arch_unmap_page(KERNEL_MAP, (usize) kernel_vm + j * PAGE_SIZE, true);
				}
				arch_user_unmap_page(process, (usize) vm + j * PAGE_SIZE, true);
			}
			vm_user_dealloc(process, vm, count, high);
			if (kernel_vm) {
				vm_kernel_dealloc(kernel_vm, count);
			}
			mutex_lock(&process->mapping_lock);
			process_remove_mapping(process, (usize) vm);
			mutex_unlock(&process->mapping_lock);
			return NULL;
		}

		if (kernel_vm) {
			arch_map_page(KERNEL_MAP, (usize) kernel_vm + i * PAGE_SIZE, page->phys, PF_READ | PF_WRITE);
		}
		if (!arch_user_map_page(process, (usize) vm + i * PAGE_SIZE, page->phys, flags)) {
			for (usize j = 0; j < i; ++j) {
				Page* p = page_from_addr(arch_virt_to_phys(process->map, (usize) vm + j * PAGE_SIZE));
				pfree(p, 1);

				if (kernel_vm) {
					arch_unmap_page(KERNEL_MAP, (usize) kernel_vm + j * PAGE_SIZE, true);
				}
				arch_user_unmap_page(process, (usize) vm + j * PAGE_SIZE, true);
			}
			vm_user_dealloc(process, vm, count, high);
			if (kernel_vm) {
				vm_kernel_dealloc(kernel_vm, count);
			}
			mutex_lock(&process->mapping_lock);
			process_remove_mapping(process, (usize) vm);
			mutex_unlock(&process->mapping_lock);
			return NULL;
		}
	}

	return vm;
}

void* vm_user_create_cow(Process* process, void* original_map, Mapping* original, void* at) {
	usize pages = ALIGNUP(original->size, PAGE_SIZE) / PAGE_SIZE;
	void* vm = vm_user_alloc(process, at, pages, true);
	if (!vm) {
		return NULL;
	}

	PageFlags page_flags = PF_USER;
	if ((original->flags & MAPPING_FLAG_R) || (original->flags & MAPPING_FLAG_W)) {
		page_flags |= PF_READ;
	}
	if (original->flags & MAPPING_FLAG_X) {
		page_flags |= PF_EXEC;
	}

	for (usize i = 0; i < pages; ++i) {
		usize phys = arch_virt_to_phys(original_map, (usize) original->base + i * PAGE_SIZE);
		if (phys) {
			Page* page = page_from_addr(phys);
			page->refs += 1;
			arch_map_page(process->map, (usize) vm + i * PAGE_SIZE, phys, page_flags);
		}
	}

	MappingFlags flags = original->flags | MAPPING_FLAG_COW;

	mutex_lock(&process->mapping_lock);
	if (!process_add_mapping(process, (usize) vm, original->size, flags)) {
		mutex_unlock(&process->mapping_lock);

		for (usize i = 0; i < pages; ++i) {
			arch_unmap_page(process->map, (usize) vm + i * PAGE_SIZE, true);
		}

		vm_user_dealloc(process, vm, pages, true);
		return NULL;
	}
	mutex_unlock(&process->mapping_lock);
	return vm;
}

void* vm_user_alloc_on_demand(Process* process, void* at, usize count, MappingFlags flags, bool high, void** kernel_mapping) {
	void* vm = vm_user_alloc(process, at, count, high);
	if (!vm) {
		return NULL;
	}
	void* kernel_vm = NULL;
	if (kernel_mapping) {
		*kernel_mapping = vm_kernel_alloc(count);
		if (!*kernel_mapping) {
			vm_user_dealloc(process, vm, count, high);
			return NULL;
		}
		kernel_vm = *kernel_mapping;
	}

	flags |= MAPPING_FLAG_ON_DEMAND;

	mutex_lock(&process->mapping_lock);
	if (!process_add_mapping(process, (usize) vm, count * PAGE_SIZE, flags)) {
		mutex_unlock(&process->mapping_lock);
		vm_user_dealloc(process, vm, count, high);
		if (kernel_vm) {
			vm_kernel_dealloc(kernel_vm, count);
		}
		return NULL;
	}
	mutex_unlock(&process->mapping_lock);
	return vm;
}

void vm_user_dealloc_kernel(void* kernel_mapping, usize count) {
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) kernel_mapping + i * PAGE_SIZE;
		arch_unmap_page(KERNEL_MAP, virt, true);
	}
}

bool vm_user_dealloc_backed(Process* process, void* ptr, usize count, bool high, void* kernel_mapping) {
	if (!process_remove_mapping(process, (usize) ptr)) {
		return false;
	}
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		usize phys = arch_virt_to_phys(process->map, virt);
		if (kernel_mapping) {
			arch_unmap_page(KERNEL_MAP, (usize) kernel_mapping + i * PAGE_SIZE, true);
		}
		arch_user_unmap_page(process, virt, true);

		Page* page = page_from_addr(phys);
		pfree(page, 1);
	}
	vm_user_dealloc(process, ptr, count, high);
	if (kernel_mapping) {
		vm_kernel_dealloc(kernel_mapping, count);
	}
	return true;
}

bool vm_user_dealloc_on_demand(Process* process, void* ptr, usize count, bool high, void* kernel_mapping) {
	if (!process_remove_mapping(process, (usize) ptr)) {
		return false;
	}
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		usize phys = arch_virt_to_phys(process->map, virt);
		if (kernel_mapping) {
			arch_unmap_page(KERNEL_MAP, (usize) kernel_mapping + i * PAGE_SIZE, true);
		}
		if (!phys) {
			continue;
		}
		arch_user_unmap_page(process, virt, true);

		Page* page = page_from_addr(phys);
		pfree(page, 1);
	}
	vm_user_dealloc(process, ptr, count, high);
	if (kernel_mapping) {
		vm_kernel_dealloc(kernel_mapping, count);
	}
	return true;
}

bool vm_user_dealloc_cow(Process* process, void* ptr, usize count) {
	if (!process_remove_mapping(process, (usize) ptr)) {
		return false;
	}
	for (usize i = 0; i < count; ++i) {
		usize virt = (usize) ptr + i * PAGE_SIZE;
		usize phys = arch_virt_to_phys(process->map, virt);
		if (!phys) {
			continue;
		}
		arch_user_unmap_page(process, virt, true);

		Page* page = page_from_addr(phys);
		if (page->refs == 0) {
			pfree(page, 1);
		}
		else {
			page->refs -= 1;
		}
	}
	vm_user_dealloc(process, ptr, count, true);
	return true;
}
