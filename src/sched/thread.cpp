#include "thread.hpp"
#include "process.hpp"
#include "arch/cpu.hpp"

Thread::Thread(kstd::string_view name, Cpu* cpu, Process* process, void (*fn)(void *), void *arg)
	: ArchThread {fn, arg, process}, name {name}, cpu {cpu}, process {process} {
	process->add_thread(this);
}

Thread::Thread(kstd::string_view name, Cpu* cpu, Process* process) : ArchThread {}, name {name}, cpu {cpu}, process {process} {
	process->add_thread(this);
}

void Thread::sleep_for(u64 us) const {
	IrqGuard guard {};
	cpu->scheduler.sleep(us);
}
