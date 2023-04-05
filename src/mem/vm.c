#include "vm.h"
#include "page.h"
#include "vmem.h"

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