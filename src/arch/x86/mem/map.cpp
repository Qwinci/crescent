#include "map.hpp"
#include "mem/page_allocator.hpp"
#include "arch/map.hpp"
#include "mem/allocator.hpp"
#include "cstring.hpp"
#include "new.hpp"
#include "misc.hpp"

PageMap* PageMap::create_user() {
	auto res = ALLOCATOR.alloc<X86PageMap>();
	if (res) {
		new (res) X86PageMap {};
		if (!res->pml4_page) {
			delete res;
			res = nullptr;
		}
		else {
#ifdef CONFIG_HHDM
			memcpy(
				arch_to_virt(res->pml4_page->phys + 0x1000 / 2),
				arch_to_virt(dyn_cast<X86PageMap>(KERNEL_MAP)->pml4_page->phys),
				0x1000 / 2);
#else
#error create_user doesn't work without hhdm
#endif
		}
	}
	return res;
}

bool X86PageMap::map(void* virt, usize phys, usize size, PageFlags::Type flags) {
	return false;
}

bool X86PageMap::unmap(void* virt, usize size) {
	return false;
}

void X86PageMap::use() {
	asm volatile("movq %0, %%cr3" : : "r"(pml4_page->phys) : "memory");
}
