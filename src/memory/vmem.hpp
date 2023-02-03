#pragma once
#include "types.hpp"
#include "utils.hpp"

void vm_kernel_init(usize base, usize size);
void* vm_kernel_alloc(usize count);
void vm_kernel_dealloc(void* ptr, usize count);
void* vm_kernel_alloc_backed(usize count);
void vm_kernel_dealloc_backed(void* ptr, usize count);

enum class AllocFlags : u32 {
	None,
	Rw,
	Exec
};

constexpr AllocFlags operator|(AllocFlags lhs, AllocFlags rhs) {
	return as<AllocFlags>(as<u32>(lhs) | as<u32>(rhs));
}

constexpr bool operator&(AllocFlags lhs, AllocFlags rhs) {
	return as<u32>(lhs) & as<u32>(rhs);
}

class PageMap;

void vm_user_init(usize size);
void* vm_user_alloc(usize count);
void vm_user_dealloc(void* ptr, usize count);
void* vm_user_alloc_backed(PageMap* map, usize count, AllocFlags flags = AllocFlags::None);
void vm_user_dealloc_backed(PageMap* map, void* ptr, usize count);
void* vm_user_alloc_kernel_mapping(PageMap* map, void* ptr, usize count);
void vm_user_dealloc_kernel_mapping(PageMap* map, void* ptr, usize count);