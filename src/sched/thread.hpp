#pragma once
#include "arch/arch_thread.hpp"
#include "double_list.hpp"
#include "string.hpp"

struct Cpu;
struct Process;

struct Thread : public ArchThread {
	Thread(kstd::string_view name, Cpu* cpu, Process* process, void (*fn)(void*), void* arg);
	Thread(kstd::string_view name, Cpu* cpu, Process* process);

	void sleep_for(u64 us) const;

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
	usize sleep_end {};
	// SCHED_LEVELS - 1
	usize level_index {11};
	Status status {Status::Waiting};
	bool exited {};
};
