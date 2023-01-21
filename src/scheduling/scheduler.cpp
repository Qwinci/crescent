#include "scheduler.hpp"

Process* ready_to_run_tasks {};
Process* ready_to_run_tasks_end {};
Process* current_task {};

Process* create_kernel_task(void (*task)()) {
	u8* stack = new u8[0x1000];
	stack += 0x1000 - 8;
	// rip
	*(u64*) stack = (u64) task;
	stack -= 8;
	// rbx
	*(u64*) stack = 0;
	stack -= 8;
	// rbp
	*(u64*) stack = 0;
	stack -= 8;
	// r12
	*(u64*) stack = 0;
	stack -= 8;
	// r13
	*(u64*) stack = 0;
	stack -= 8;
	// r14
	*(u64*) stack = 0;
	stack -= 8;
	// r15
	*(u64*) stack = 0;

	auto* process = new Process();
	process->rsp = (u64) stack;
	process->kernel_rsp = (u64) new u8[0x1000];
	process->kernel_rsp += 0x1000;
	process->map = VirtAddr {get_map()}.to_phys().as_usize();
	process->state = ProcessState::ReadyToRun;
	return process;
}