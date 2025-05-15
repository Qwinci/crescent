#include "posix_sys.hpp"
#include "sched/process.hpp"

#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4

#define MAP_SHARED 1
#define MAP_PRIVATE 2

#define MAP_FIXED 0x10
#define MAP_ANONYMOUS 0x20
#define MAP_32BIT 0x40

void posix_mmap(SyscallFrame* frame) {
	void* hint = reinterpret_cast<void*>(*frame->arg0());
	usize size = *frame->arg1();
	int prot = static_cast<int>(*frame->arg2());
	int flags = static_cast<int>(*frame->arg3());
	int fd = static_cast<int>(*frame->arg4());
	auto offset = static_cast<off64_t>(*frame->arg5());

	if (!(flags & (MAP_PRIVATE | MAP_SHARED))) {
		*frame->error() = EINVAL;
		return;
	}

	assert(!(flags & MAP_SHARED));
	assert(fd == -1);
	assert(offset == 0);

	MemoryAllocFlags alloc_flags {};
	if (flags & MAP_ANONYMOUS) {
		alloc_flags |= MemoryAllocFlags::Demand;
	}
	if (flags & MAP_FIXED) {
		assert(flags & MAP_ANONYMOUS);
		alloc_flags |= MemoryAllocFlags::Fixed;
	}

	PageFlags new_prot {};
	if (prot & PROT_READ) {
		new_prot |= PageFlags::Read;
	}
	if (prot & PROT_WRITE) {
		new_prot |= PageFlags::Write;
	}
	if (prot & PROT_EXEC) {
		new_prot |= PageFlags::Execute;
	}

	auto thread = get_current_thread();

	auto alloc_addr = thread->process->allocate(hint, size, new_prot, alloc_flags, nullptr);
	if (!alloc_addr) {
		*frame->error() = ENOMEM;
		return;
	}

	*frame->error() = 0;
	*frame->ret() = alloc_addr;
}

void posix_munmap(SyscallFrame* frame) {
	usize addr = *frame->arg0();
	usize size = *frame->arg1();

	auto thread = get_current_thread();
	if (!thread->process->free(addr, size)) {
		*frame->error() = EINVAL;
		return;
	}

	*frame->error() = 0;
}

void posix_mprotect(SyscallFrame* frame) {
	usize addr = *frame->arg0();
	usize size = *frame->arg1();
	int prot = static_cast<int>(*frame->arg2());

	PageFlags new_prot {};
	if (prot & PROT_READ) {
		new_prot |= PageFlags::Read;
	}
	if (prot & PROT_WRITE) {
		new_prot |= PageFlags::Write;
	}
	if (prot & PROT_EXEC) {
		new_prot |= PageFlags::Execute;
	}

	auto thread = get_current_thread();
	if (!thread->process->protect(addr, size, new_prot)) {
		*frame->error() = EINVAL;
		return;
	}

	*frame->error() = 0;
}
