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

static void after_switch_from(Task* old_task, Task* this_task) {
	auto local = get_cpu_local();

	local->tss.rsp0_low = as<u32>(this_task->kernel_rsp);
	local->tss.rsp0_high = as<u32>(this_task->kernel_rsp >> 32);

	this_task->status = TaskStatus::Running;

	if (old_task->status == TaskStatus::Running) {
		sched_queue_task(old_task);
	}
	else if (old_task->status == TaskStatus::Exited || old_task->status == TaskStatus::Killed) {
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
	get_cpu_local()->thread_count += 1;
	after_switch_from(old_task, get_cpu_local()->current_task);
}

struct InitStackFrame {
	u64 r15, r14, r13, r12, rbp, rbx;
	decltype(after_init_switch)* after_init_switch;
	void (*fn)();
	void* arg;
	u64 null_rbp;
	u64 null_rip;
};

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

Task* create_kernel_task(const char* name, void (*fn)(), void* arg) {
	auto task = new Task;

	u8* stack = new u8[0x2000]();
	task->stack_base = cast<u64>(stack);
	task->stack_size = 0x2000;

	auto* init_frame = cast<InitStackFrame*>(stack + 0x2000 - sizeof(InitStackFrame));

	stack += 0x2000 - sizeof(InitStackFrame);

	init_frame->after_init_switch = after_init_switch;
	init_frame->fn = fn;
	init_frame->arg = arg;
	init_frame->null_rbp = 0;
	init_frame->null_rip = 0;

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
	auto local = get_cpu_local();

	auto& level = local->levels[level_num];

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

	auto local = get_cpu_local();

	auto prev = switch_task(this_task, new_task);

	if (prev->map != local->current_task->map) {
		cast<PageMap*>(PhysAddr {local->current_task->map}.to_virt().as_usize())->load();
	}

	after_switch_from(prev, local->current_task);
}

void sched() {
	auto flags = enter_critical();

	auto local = get_cpu_local();

	Task* task = nullptr;
	for (u8 i = SCHED_MAX_LEVEL; i > 0; --i) {
		if (!local->levels[i - 1].is_empty()) {
			task = get_next_task_from_level(i - 1);
			break;
		}
	}

	if (!task) {
		if (local->current_task->status != TaskStatus::Running) {
			panic("todo idle");
		}
		else {
			return;
		}
	}

	auto this_task = local->current_task;
	local->current_task = task;
	switch_wrapper(this_task, local->current_task);

	leave_critical(flags);
}

void sched_queue_task(Task* task) {
	auto local = get_cpu_local();

	auto& level = local->levels[task->task_level];

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

void sched_block(TaskStatus status) {
	auto flags = enter_critical();
	get_cpu_local()->current_task->status = status;
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

	auto local = get_cpu_local();

	local->current_task->status = TaskStatus::Sleeping;
	local->current_task->sleep_end = end;

	if (!sleeping_tasks) {
		sleeping_tasks = local->current_task;
		local->current_task->next = nullptr;
	}
	else {
		Task* task;
		for (task = sleeping_tasks; task->next; task = task->next) {
			if (task->next->sleep_end > end) {
				break;
			}
		}
		local->current_task->next = task->next;
		task->next = local->current_task;
	}

	leave_critical(flags);
	sched_block(TaskStatus::Sleeping);
}

[[noreturn]] void sched_exit() {
	auto local = get_cpu_local();
	local->current_task->status = TaskStatus::Exited;
	local->thread_count -= 1;
	sched();
	__builtin_unreachable();
}

void sched_kill() {
	auto local = get_cpu_local();
	local->current_task->status = TaskStatus::Killed;
	local->thread_count -= 1;
	sched();
	__builtin_unreachable();
}

[[noreturn]] void test_task() {
	for (usize i = 0; i < 200; ++i) {
		println("test task");
		sched_sleep(1000 * 1000);
	}
	sched_exit();
}

static void sched_load_balance() {
	auto local = get_cpu_local();

	usize min_thread_count = 0xFF;
	u8 min_cpu = 0;
	usize max_thread_count = 0;
	u8 max_cpu = 0;

	for (usize i = 0; i < cpu_count; ++i) {
		auto& cpu = cpu_locals[i];
		u32 thread_count = cpu.thread_count;
		if (thread_count < min_thread_count) {
			min_thread_count = thread_count;
			min_cpu = i;
		}
		if (thread_count > max_thread_count) {
			max_thread_count = thread_count;
			max_cpu = i;
		}
	}

	if (min_cpu != max_cpu && min_thread_count != max_thread_count) {
		auto diff = max_thread_count - min_thread_count;
		println("moving ", diff / 2, " threads from cpu ", max_cpu, " to cpu ", min_cpu);
	}

	println("sched load balance");

	local->timer.create_timer(get_timer_us() + US_IN_SEC, sched_load_balance);
}

void sched_init() {
	auto this_task = new Task;
	this_task->status = TaskStatus::Running;
	this_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	memcpy(this_task->name, "kernel main", sizeof("kernel main"));
	this_task->next = nullptr;
	this_task->task_level = SCHED_MAX_LEVEL - 1;
	get_cpu_local()->current_task = this_task;

	constexpr usize inc = (SCHED_SLICE_MAX_US - SCHED_SLICE_MIN_US) / SCHED_MAX_LEVEL;

	auto local = get_cpu_local();

	for (u8 i = 0; i < SCHED_MAX_LEVEL; ++i) {
		local->levels[i].slice_us = (SCHED_MAX_LEVEL - i) * inc;
	}

	local->timer.create_timer(get_timer_us() + US_IN_SEC, sched_load_balance);
}