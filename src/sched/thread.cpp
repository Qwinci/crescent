#include "thread.hpp"
#include "process.hpp"
#include "arch/cpu.hpp"

Thread::Thread(kstd::string_view name, Cpu* cpu, Process* process, void (*fn)(void *), void *arg)
	: ArchThread {fn, arg, process}, name {name}, cpu {cpu}, process {process} {
	process->add_thread(this);
}

Thread::Thread(kstd::string_view name, Cpu* cpu, Process* process, const SysvInfo& sysv)
	: ArchThread {sysv, process}, name {name}, cpu {cpu}, process {process} {
	process->add_thread(this);
}

Thread::Thread(kstd::string_view name, Cpu* cpu, Process* process) : ArchThread {}, name {name}, cpu {cpu}, process {process} {
	process->add_thread(this);
}

void Thread::sleep_for(u64 ns) const {
	IrqGuard guard {};
	auto state = cpu->scheduler.prepare_for_sleep(ns);
	cpu->scheduler.sleep(state);
}

void Thread::yield() const {
	IrqGuard guard {};
	cpu->scheduler.yield();
}

void Thread::add_descriptor(ThreadDescriptor* descriptor) {
	IrqGuard irq_guard {};
	descriptors.lock()->push(descriptor);
}

void Thread::remove_descriptor(ThreadDescriptor* descriptor) {
	IrqGuard irq_guard {};
	descriptors.lock()->remove(descriptor);
}

void Thread::exit(int exit_status, ThreadDescriptor* skip_lock) {
	IrqGuard irq_guard {};
	auto guard = descriptors.lock();
	for (auto& desc : *guard) {
		if (&desc != skip_lock) {
			auto thread_guard = desc.thread.lock();
			assert(thread_guard == this);
			*thread_guard = nullptr;
		}
		else {
			assert(desc.thread.get_unsafe() == this);
			desc.thread.get_unsafe() = nullptr;
		}

		desc.exit_status = exit_status;
	}
	guard->clear();
}

ThreadDescriptor::~ThreadDescriptor() {
	auto guard = thread.lock();
	if (guard) {
		(*guard)->remove_descriptor(this);
	}
}
