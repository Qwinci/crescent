#include "console.hpp"
#include "memory/memory.hpp"
#include "memory/pmm.hpp"
#include "memory/std.hpp"
#include "memory/vmem.hpp"
#include "sched/sched.hpp"
#include "types.hpp"

extern "C" const usize syscall_handler_count = 4;

[[noreturn]] void sys_exit(int code);
i64 sys_fbmn_cur_info(u32* width, u32* height, u32* depth);
void* sys_fbmn_create(u32 width, u32 height, u32 depth);
i64 sys_fbmn_swap(void* fb, u32 width, u32 height, u32 depth);

// 0 sys_exit
// 1 sys_fbmn

void* syscall_handlers[syscall_handler_count] {
		reinterpret_cast<void*>(&sys_exit),
		reinterpret_cast<void*>(&sys_fbmn_cur_info),
		reinterpret_cast<void*>(&sys_fbmn_create),
		reinterpret_cast<void*>(&sys_fbmn_swap)
};

void syscall_start() {
	get_map()->load();
}

void syscall_end() {
	current_task->get_map()->load();
}

[[noreturn]] void sys_exit(int code) {
	println("sys_exit called with code ", code);
	sched_exit();
}

#define ENOEX(type) (type) (-1LL)
#define EINVAL(type) (type) (-2LL)
#define ENOSUP(type) (type) (-3LL)

static inline void* kptr(void* uptr) {
	auto base = ALIGNDOWN(cast<usize>(uptr), PAGE_SIZE);
	auto ptr_offset = cast<usize>(uptr) - base;
	auto phys_base = current_task->get_map()->virt_to_phys(VirtAddr {base});
	if (phys_base.as_usize() == 0) {
		return nullptr;
	}
	return phys_base.offset(ptr_offset).to_virt();
}

#define VERIFY_PTR(name, uptr, error) auto name = as<decltype(uptr)>(kptr(uptr)); \
if (!(name)) return error

i64 sys_fbmn_cur_info(u32* width, u32* height, u32* depth) {
	VERIFY_PTR(kwidth, width, EINVAL(i64));
	VERIFY_PTR(kheight, height, EINVAL(i64));
	VERIFY_PTR(kdepth, depth, EINVAL(i64));

	syscall_start();

	*kwidth = current_fb.width;
	*kheight = current_fb.height;
	*kdepth = 32;

	println("sys_fbmn_cur_info");

	syscall_end();
	return 0;
}

void* sys_fbmn_create(u32 width, u32 height, u32 depth) {
	if (width == 0 || height == 0 || depth == 0) {
		return EINVAL(void*);
	}
	syscall_start();

	auto size = ALIGNUP(width * height * (depth / 8), PAGE_SIZE);
	auto fb = vm_user_alloc_backed(current_task->get_map(), size / PAGE_SIZE, AllocFlags::Rw);

	auto kmapping = vm_user_alloc_kernel_mapping(current_task->get_map(), fb, size / PAGE_SIZE);
	memset(kmapping, 0, size);
	vm_user_dealloc_kernel_mapping(current_task->get_map(), kmapping, size / PAGE_SIZE);

	println("sys_fbmn_create");

	syscall_end();
	return fb;
}

i64 sys_fbmn_swap(void* fb, u32 width, u32 height, u32 depth) {
	if (width == 0 || height == 0 || depth == 0) {
		return EINVAL(i64);
	}
	else if (width > current_fb.width || height > current_fb.height || depth > 32) {
		return EINVAL(i64);
	}

	syscall_start();

	auto size = ALIGNUP(width * height * (depth / 8), PAGE_SIZE);
	println("sys_fbmn_swap (start)");
	auto kmapping = vm_user_alloc_kernel_mapping(current_task->get_map(), fb, size / PAGE_SIZE);
	if (!kmapping) {
		println("sys_fbmn_swap (end, failed)");
		syscall_end();
		return EINVAL(i64);
	}

	if (width == current_fb.width && height == current_fb.height && depth == 32) {
		for (usize y = 0; y < height; ++y) {
			auto src = cast<void*>(cast<usize>(kmapping) + y * width * 4);
			auto dest = cast<void*>(current_fb.address + current_fb.pitch * y);
			memcpy(dest, src, width * 4);
		}
	}
	else {
		syscall_end();
		return ENOSUP(i64);
	}

	vm_user_dealloc_kernel_mapping(current_task->get_map(), kmapping, size / PAGE_SIZE);

	println("sys_fbmn_swap (end)");

	syscall_end();

	return 0;
}