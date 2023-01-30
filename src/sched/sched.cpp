#include "sched.hpp"
#include "console.hpp"
#include "cpu/cpu.hpp"
#include "memory/std.hpp"
#include "timer/timer_int.hpp"
#include "utils.hpp"

Task* current_task {};
Task* ready_tasks {};
Task* ready_tasks_end {};

static void after_switch_from(Task* old_task, Task* this_task) {
	println("switched from '", (const char*) old_task->name, "' to '", (const char*) this_task->name, "'");

	auto local = get_cpu_local();

	local->tss.rsp0_low = as<u32>(this_task->kernel_rsp);
	local->tss.rsp0_high = as<u32>(this_task->kernel_rsp >> 32);

	this_task->status = TaskStatus::Running;

	if (old_task->map != this_task->map) {
		cast<PageMap*>(this_task->map)->load();
	}

	if (old_task->status == TaskStatus::Running) {
		sched_queue_task(old_task);
	}
}

static void after_init_switch(Task* old_task) {
	after_switch_from(old_task, current_task);
}

Task* create_kernel_task(const char* name, void (*fn)()) {
	u8* stack = new u8[0x2000]();
	stack += 0x2000 - sizeof(fn) * 2;
	*cast<decltype(fn)*>(stack) = fn;
	stack -= sizeof(fn);
	*cast<decltype(&after_init_switch)*>(stack) = after_init_switch;
	// space for 6 registers (all zeros)
	stack -= 6 * 8;

	auto task = new Task;
	memcpy(task->name, name, 128);
	task->status = TaskStatus::Ready;

	task->rsp = cast<u64>(stack);
	task->next = nullptr;
	task->map = VirtAddr {get_map()}.to_phys().as_usize();

	return task;
}

/// Returns the task that switches back to us
extern "C" Task* switch_task(Task* old_task, Task* new_task);

void sched() {
	if (ready_tasks) {
		auto task = ready_tasks;
		ready_tasks = task->next;
		println("switching to task '", as<const char*>(task->name), "'");
		auto this_task = current_task;
		current_task = task;
		auto prev = switch_task(this_task, task);
		after_switch_from(prev, current_task);
	}
	else {
		panic("todo idle");
	}
}

void sched_queue_task(Task* task) {
	if (!ready_tasks) {
		ready_tasks = task;
		ready_tasks_end = task;
	}
	else {
		ready_tasks_end->next = task;
		ready_tasks_end = task;
	}
}

Task* sleeping_tasks {};
Task* sleeping_tasks_end {};
static Spinlock sched_misc_lock {};

static void block_task() {
	sched_misc_lock.unlock();
	sched();
}

void unblock_task(Task* task) {
	// todo priority if should replace current task
	sched_queue_task(task);
	sched();
}

void sched_sleep(u64 us) {
	auto end = get_timer_us() + us;
	sched_misc_lock.lock();

	current_task->status = TaskStatus::Sleeping;
	current_task->sleep_end = us;

	if (!sleeping_tasks) {
		sleeping_tasks = current_task;
		sleeping_tasks_end = current_task;
		current_task->next = nullptr;
		block_task();
	}
	else {
		for (auto task = sleeping_tasks; task->next; task = task->next) {
			if (task->next->sleep_end > end) {
				current_task->next = task->next;
				task->next = current_task;
				block_task();
			}
		}
		sleeping_tasks_end->next = current_task;
		sleeping_tasks_end = current_task;
		current_task->next = nullptr;
		block_task();
	}
}

[[noreturn]] void test_task() {
	while (true) {
		println("test task");
		//sched_sleep(1000 * 1000);
		sched();
	}
}

void sched_init() {
	auto this_task = new Task;
	this_task->status = TaskStatus::Running;
	this_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	memcpy(this_task->name, "kernel main", sizeof("kernel main"));
	this_task->next = nullptr;
	current_task = this_task;
}

void test_sched() {
	auto this_task = new Task;
	this_task->status = TaskStatus::Running;
	this_task->map = VirtAddr {get_map()}.to_phys().as_usize();
	this_task->kernel_rsp = 1234;
	memcpy(this_task->name, "kernel main", sizeof("kernel main"));
	this_task->next = nullptr;
	current_task = this_task;

	auto task = create_kernel_task("test task", test_task);
	sched_queue_task(task);
	sched();
}