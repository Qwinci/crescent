#include "sched.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "memory/memory.hpp"
#include "memory/pmm.hpp"
#include "memory/vmem.hpp"
#include "std/string.h"
#include "timer/timer_int.hpp"
#include "usermode/usermode.hpp"
#include "utils.hpp"

Task* current_task {};
SchedLevel levels[SCHED_MAX_LEVEL] {};

static void after_switch_from(Task* old_task, Task* this_task) {
	auto local = get_cpu_local();

	local->tss.rsp0_low = as<u32>(this_task->kernel_rsp);
	local->tss.rsp0_high = as<u32>(this_task->kernel_rsp >> 32);

	this_task->status = TaskStatus::Running;

	if (old_task->status == TaskStatus::Running) {
		sched_queue_task(old_task);
	}
	else if (old_task->status == TaskStatus::Exited) {
		if (old_task->user) {
			ALLOCATOR.dealloc(cast<void*>(old_task->kernel_rsp - 0x2000), 0x2000);

			auto* map = cast<PageMap*>(PhysAddr {old_task->map}.to_virt().as_usize());

			vm_user_dealloc_backed(map, cast<void*>(old_task->stack_base), ALIGNUP(old_task->stack_size, 0x1000) / 0x1000);

			map->destroy_lower_half();
			ALLOCATOR.dealloc(map, sizeof(*map));
		}
		else {
			ALLOCATOR.dealloc(cast<void*>(old_task->stack_base), old_task->stack_size);
		}
		ALLOCATOR.dealloc(old_task, sizeof(*old_task));
	}
}

static void after_init_switch(Task* old_task) {
	after_switch_from(old_task, current_task);
}

struct UserInitStackFrame {
	u64 r15, r14, r13, r12, rbp, rbx;
	decltype(after_init_switch)* after_init_switch;
	decltype(usermode_ret)* usermode_ret;
	void (*fn)();
	void* arg;
	u64 null_rbp;
	u64 null_rip;
};

Task* create_user_task(const char* name, PageMap* map, void (*fn)(), void* arg) {
	auto task = new Task;

	u8* kernel_stack = new u8[0x2000];
	kernel_stack += 0x2000;
	task->kernel_rsp = cast<u64>(kernel_stack);

	u8* stack = cast<u8*>(vm_user_alloc_backed(map, 2, AllocFlags::Rw));
	task->stack_base = cast<u64>(stack);
	task->stack_size = 0x2000;

	u8* stack_kview = cast<u8*>(vm_user_alloc_kernel_mapping(map, stack, 2));

	auto* init_frame = cast<UserInitStackFrame*>(stack_kview + 0x2000 - sizeof(UserInitStackFrame));

	stack += 0x2000 - sizeof(UserInitStackFrame);

	init_frame->after_init_switch = after_init_switch;
	init_frame->usermode_ret = usermode_ret;
	init_frame->fn = fn;
	init_frame->arg = arg;
	init_frame->null_rbp = 0;
	init_frame->null_rip = 0;

	vm_user_dealloc_kernel_mapping(map, stack_kview, 2);

	strncpy(task->name, name, 128);
	task->status = TaskStatus::Ready;

	task->rsp = cast<u64>(stack);
	task->map = VirtAddr {map}.to_phys().as_usize();
	task->task_level = SCHED_MAX_LEVEL - 1;
	task->user = true;

	return task;
}

Task* create_kernel_task(const char* name, void (*fn)()) {
	auto task = new Task;

	u8* stack = new u8[0x2000]();
	task->stack_base = cast<u64>(stack);
	task->stack_size = 0x2000;

	stack += 0x2000 - sizeof(fn);
	*cast<decltype(fn)*>(stack) = fn;
	stack -= sizeof(fn);
	*cast<decltype(&after_init_switch)*>(stack) = after_init_switch;
	// space for 6 registers (all zeros)
	stack -= 6 * 8;

	strncpy(task->name, name, 128);
	task->status = TaskStatus::Ready;

	task->rsp = cast<u64>(stack);
	task->map = VirtAddr {get_map()}.to_phys().as_usize();
	task->task_level = SCHED_MAX_LEVEL - 1;
	task->user = false;

	return task;
}

/// Returns the task that switches back to us
extern "C" Task* switch_task(Task* old_task, Task* new_task);

static Task* get_next_task_from_level(u8 level_num) {
	auto& level = levels[level_num];

	if (level.ready_tasks) {
		auto task = level.ready_tasks;
		level.ready_tasks = task->next;
		return task;
	}
	else {
		return nullptr;
	}
}

static void switch_wrapper(Task* this_task, Task* new_task) {
	if (this_task->map != new_task->map) {
		cast<PageMap*>(PhysAddr {new_task->map}.to_virt().as_usize())->load();
	}

	auto prev = switch_task(this_task, new_task);

	if (prev->map != current_task->map) {
		cast<PageMap*>(PhysAddr {current_task->map}.to_virt().as_usize())->load();
	}

	after_switch_from(prev, current_task);
}

void sched() {
	auto flags = enter_critical();

	Task* task = nullptr;
	for (u8 i = SCHED_MAX_LEVEL; i > 0; --i) {
		if (!levels[i - 1].is_empty()) {
			task = get_next_task_from_level(i - 1);
			break;
		}
	}

	if (!task) {
		if (current_task->status != TaskStatus::Running) {
			panic("todo idle");
		}
		else {
			return;
		}
	}

	//println("switching to task '", as<const char*>(task->name), "' (level ", task->task_level, ", map ", (void*) task->map, ")");
	auto this_task = current_task;
	current_task = task;
	switch_wrapper(this_task, current_task);

	leave_critical(flags);
}

void sched_queue_task(Task* task) {
	auto& level = levels[task->task_level];

	if (!level.ready_tasks) {
		level.ready_tasks = task;
		level.ready_tasks_end = task;
	}
	else {
		level.ready_tasks_end->next = task;
		level.ready_tasks_end = task;
	}
}

Task* sleeping_tasks {};
Task* sleeping_tasks_end {};

void sched_block(TaskStatus status) {
	auto flags = enter_critical();
	current_task->status = status;
	leave_critical(flags);
	sched();
}

void sched_unblock(Task* task) {
	if (task->task_level < SCHED_MAX_LEVEL - 1) {
		task->task_level += 1;
	}
	sched_queue_task(task);
}

void sched_sleep(u64 us) {
	auto flags = enter_critical();

	auto end = get_timer_us() + us;

	current_task->status = TaskStatus::Sleeping;
	current_task->sleep_end = end;

	if (!sleeping_tasks) {
		sleeping_tasks = current_task;
		sleeping_tasks_end = current_task;
		current_task->next = nullptr;
	}
	else {
		Task* task;
		for (task = sleeping_tasks; task->next; task = task->next) {
			if (task->next->sleep_end > end) {
				break;
			}
		}
		current_task->next = task->next;
		task->next = current_task;

		if (!task->next) {
			sleeping_tasks_end = current_task;
		}
	}

	leave_critical(flags);
	sched_block(TaskStatus::Sleeping);
}

[[noreturn]] void sched_exit() {
	current_task->status = TaskStatus::Exited;
	sched();
	__builtin_unreachable();
}

void sched_kill() {
	current_task->status = TaskStatus::Killed;
	sched();
	__builtin_unreachable();
}

[[noreturn]] void test_task() {
	for (usize i = 0; i < 2; ++i) {
		println("test task");
		sched_sleep(1000 * 1000);
	}
	sched_exit();
}

void sched_init() {
	auto this_task = new Task;
	this_task->status = TaskStatus::Running;
	this_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	memcpy(this_task->name, "kernel main", sizeof("kernel main"));
	this_task->next = nullptr;
	this_task->task_level = SCHED_MAX_LEVEL - 1;
	current_task = this_task;

	constexpr usize inc = (SCHED_SLICE_MAX_US - SCHED_SLICE_MIN_US) / SCHED_MAX_LEVEL;

	for (u8 i = 0; i < SCHED_MAX_LEVEL; ++i) {
		levels[i].slice_us = (SCHED_MAX_LEVEL - i) * inc;
	}
}