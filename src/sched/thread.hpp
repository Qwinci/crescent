#pragma once
#include "arch/arch_thread.hpp"
#include "double_list.hpp"
#include "string.hpp"
#include "sysv.hpp"

struct Cpu;
struct Process;

struct Thread;

struct ThreadDescriptor {
	constexpr ThreadDescriptor(Thread* thread, int exit_status)
		: thread {thread}, exit_status {exit_status} {}

	~ThreadDescriptor();

	DoubleListHook hook {};
	Spinlock<Thread*> thread {};
	int exit_status {};
};

struct Thread : public ArchThread {
	Thread(kstd::string_view name, Cpu* cpu, Process* process, void (*fn)(void*), void* arg);
	Thread(kstd::string_view name, Cpu* cpu, Process* process, const SysvInfo& sysv);
	Thread(kstd::string_view name, Cpu* cpu, Process* process);

	void sleep_for(u64 ns) const;
	void yield() const;

	void add_descriptor(ThreadDescriptor* descriptor);
	void remove_descriptor(ThreadDescriptor* descriptor);
	void exit(int exit_status, ThreadDescriptor* skip_lock = nullptr);

	enum class Status {
		Running,
		Waiting,
		Blocked,
		Sleeping
	};

	DoubleListHook hook {};
	DoubleListHook misc_hook {};
	DoubleListHook process_hook {};
	kstd::string name;
	Cpu* cpu;
	Process* process {};
	Spinlock<DoubleList<ThreadDescriptor, &ThreadDescriptor::hook>> descriptors {};
	usize sleep_end {};
	// SCHED_LEVELS - 1
	usize level_index {0};
	Status status {Status::Waiting};
	bool exited {};
	bool pin_level {};
	bool pin_cpu {};
	Spinlock<void> move_lock {};
	Spinlock<void> status_lock {};
	bool sleep_interrupted {};
	bool dont_block {};
	uint32_t thread_id {};
};

#ifdef __x86_64__
// used by cpu_var.hpp
static_assert(offsetof(Thread, cpu) == 160);
// used by arch_sched.cpp
static_assert(offsetof(Thread, move_lock) == 223);
#elif defined(__aarch64__)
// used by cpu_var.hpp
static_assert(offsetof(Thread, cpu) == 136);
// used by arch_sched.cpp
static_assert(offsetof(Thread, move_lock) == 200);
#endif
