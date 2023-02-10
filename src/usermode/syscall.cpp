#include "console.hpp"
#include "cpu/cpu.hpp"
#include "memory/memory.hpp"
#include "memory/pmm.hpp"
#include "memory/std.hpp"
#include "memory/vmem.hpp"
#include "sched/sched.hpp"
#include "types.hpp"

extern "C" const usize syscall_handler_count = 4;

[[noreturn]] void sys_exit(int code);
i32 sys_fbmn_cur_info(u32* width, u32* height, u32* depth);
void* sys_fbmn_create(u32 width, u32 height, u32 depth);
i32 sys_fbmn_swap(void* fb, u32 width, u32 height, u32 depth);

// 0 sys_exit
// 1 sys_fbmn_cur_info
// 2 sys_fbmn_create
// 3 sys_fbmn_swap

void* syscall_handlers[syscall_handler_count] {
		reinterpret_cast<void*>(&sys_exit),
		reinterpret_cast<void*>(&sys_fbmn_cur_info),
		reinterpret_cast<void*>(&sys_fbmn_create),
		reinterpret_cast<void*>(&sys_fbmn_swap)
};

[[noreturn]] void sys_exit(int code) {
	println("sys_exit called with code ", code);
	sched_exit();
}

static inline void* kptr(void* uptr) {
	auto base = ALIGNDOWN(cast<usize>(uptr), PAGE_SIZE);
	auto ptr_offset = cast<usize>(uptr) - base;
	auto phys_base = get_cpu_local()->current_task->get_map()->virt_to_phys(VirtAddr {base});
	if (phys_base.as_usize() == 0) {
		return nullptr;
	}
	return phys_base.offset(ptr_offset).to_virt();
}

static inline const void* kptr(const void* uptr) {
	auto base = ALIGNDOWN(cast<usize>(uptr), PAGE_SIZE);
	auto ptr_offset = cast<usize>(uptr) - base;
	auto phys_base = get_cpu_local()->current_task->get_map()->virt_to_phys(VirtAddr {base});
	if (phys_base.as_usize() == 0) {
		return nullptr;
	}
	return phys_base.offset(ptr_offset).to_virt();
}

#define VERIFY_PTR(name, uptr, error) auto name = as<decltype(uptr)>(kptr(uptr)); \
if (!(name)) return error

#define ENOEX(type) (type) (-1LL)
#define EINVAL(type) (type) (-2LL)
#define ENOSUP(type) (type) (-3LL)

i32 sys_fbmn_cur_info(u32* width, u32* height, u32* depth) {
	VERIFY_PTR(kwidth, width, EINVAL(i32));
	VERIFY_PTR(kheight, height, EINVAL(i32));
	VERIFY_PTR(kdepth, depth, EINVAL(i32));

	*kwidth = current_fb.width;
	*kheight = current_fb.height;
	*kdepth = 32;

	println("sys_fbmn_cur_info");

	return 0;
}

void* sys_fbmn_create(u32 width, u32 height, u32 depth) {
	if (width == 0 || height == 0 || depth == 0) {
		return EINVAL(void*);
	}

	auto size = ALIGNUP(width * height * (depth / 8), PAGE_SIZE);
	auto fb = vm_user_alloc_backed(get_cpu_local()->current_task->get_map(), size / PAGE_SIZE, AllocFlags::Rw);
	if (!fb) {
		return nullptr;
	}

	memset(fb, 0, size);

	println("sys_fbmn_create");

	return fb;
}

i32 sys_fbmn_swap(void* fb, u32 width, u32 height, u32 depth) {
	if (width == 0 || height == 0 || depth == 0) {
		return EINVAL(i32);
	}
	else if (width > current_fb.width || height > current_fb.height || depth > 32) {
		return EINVAL(i32);
	}

	auto size = ALIGNUP(width * height * (depth / 8), PAGE_SIZE);
	println("sys_fbmn_swap (start)");
	auto map = get_cpu_local()->current_task->get_map();
	for (usize i = 0; i < size; i += PAGE_SIZE) {
		if (!map->virt_to_phys(VirtAddr {fb}.offset(i)).as_usize()) {
			println("sys_fbmn_swap (end, failed)");
			return EINVAL(i32);
		}
	}

	if (width == current_fb.width && height == current_fb.height && depth == 32) {
		for (usize y = 0; y < height; ++y) {
			auto src = cast<void*>(cast<usize>(fb) + y * width * 4);
			auto dest = cast<void*>(current_fb.address + current_fb.pitch * y);
			memcpy(dest, src, width * 4);
		}
	}
	else {
		return ENOSUP(i32);
	}

	println("sys_fbmn_swap (end)");

	return 0;
}