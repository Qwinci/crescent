#pragma once
#include "arch/paging.hpp"
#include "mem/vmem.hpp"
#include "rb_tree.hpp"
#include "compare.hpp"
#include "sched/sched.hpp"
#include "manually_init.hpp"
#include "handle_table.hpp"
#include "sched/cpu_set.hpp"

struct IpcSocket;

enum class MemoryAllocFlags {
	Read = 1 << 0,
	Write = 1 << 1,
	Execute = 1 << 2,
	Backed = 1 << 3
};

constexpr MemoryAllocFlags operator|(MemoryAllocFlags lhs, MemoryAllocFlags rhs) {
	return static_cast<MemoryAllocFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
constexpr MemoryAllocFlags& operator|=(MemoryAllocFlags& lhs, MemoryAllocFlags rhs) {
	lhs = lhs | rhs;
	return lhs;
}
constexpr bool operator&(MemoryAllocFlags lhs, MemoryAllocFlags rhs) {
	return static_cast<int>(lhs) & static_cast<int>(rhs);
}

struct UniqueKernelMapping {
	constexpr UniqueKernelMapping() = default;
	constexpr UniqueKernelMapping(UniqueKernelMapping&& other)
		: ptr {other.ptr}, size {other.size} {
		other.ptr = nullptr;
		other.size = 0;
	}
	constexpr UniqueKernelMapping(const UniqueKernelMapping&) = delete;
	constexpr UniqueKernelMapping& operator=(UniqueKernelMapping&& other) {
		this->~UniqueKernelMapping();
		ptr = other.ptr;
		size = other.size;
		other.ptr = nullptr;
		other.size = 0;
		return *this;
	}
	constexpr UniqueKernelMapping& operator=(const UniqueKernelMapping&) = delete;

	constexpr void* data() {
		return ptr;
	}

	[[nodiscard]] constexpr const void* data() const {
		return ptr;
	}

	~UniqueKernelMapping();

private:
	friend struct Process;

	constexpr UniqueKernelMapping(void* ptr, usize size) : ptr {ptr}, size {size} {}

	void* ptr {};
	usize size {};
};

struct ProcessDescriptor {
	constexpr ProcessDescriptor(Process* process, int exit_status)
		: process {process}, exit_status {exit_status} {}
	~ProcessDescriptor();

	kstd::shared_ptr<ProcessDescriptor> duplicate();

	DoubleListHook hook {};
	Spinlock<Process*> process {};
	int exit_status {};
};

struct Process {
	explicit Process(kstd::string_view name, bool user, Handle&& stdin, Handle&& stdout, Handle&& stderr);
	~Process();

	usize allocate(void* base, usize size, MemoryAllocFlags flags, UniqueKernelMapping* mapping, CacheMode cache_mode = CacheMode::WriteBack);
	void free(usize ptr, usize size);

	void add_thread(Thread* thread);
	void remove_thread(Thread* thread);

	void add_descriptor(ProcessDescriptor* descriptor);
	void remove_descriptor(ProcessDescriptor* descriptor);
	void exit(int status, ProcessDescriptor* skip_lock = nullptr);

	[[nodiscard]] bool handle_pagefault(usize addr);

	[[nodiscard]] inline bool is_empty() {
		return threads.lock()->is_empty();
	}

	kstd::string name;

	PageMap page_map;
	HandleTable handles {};
	struct Service* service {};
	Spinlock<kstd::shared_ptr<IpcSocket>> ipc_socket {nullptr};
	bool user;
	bool killed {};
	Spinlock<DoubleList<Thread, &Thread::process_hook>> threads {};
	Spinlock<CpuSet> cpu_set {};

	struct Futex {
		RbTreeHook hook {};
		kstd::atomic<int>* ptr {};
		DoubleList<Thread, &Thread::misc_hook> waiters {};

		constexpr int operator<=>(const Futex& other) const {
			return kstd::threeway(ptr, other.ptr);
		}
	};

	struct Mapping {
		RbTreeHook hook {};
		usize base {};
		usize size {};
		MemoryAllocFlags flags {};

		constexpr int operator<=>(const Mapping& other) const {
			return kstd::threeway(base, other.base);
		}
	};

	Spinlock<RbTree<Futex, &Futex::hook>> futexes {};
	IrqSpinlock<RbTree<Mapping, &Mapping::hook>> mappings {};

private:
	VMem vmem {};
	Spinlock<DoubleList<ProcessDescriptor, &ProcessDescriptor::hook>> descriptors {};
	uint32_t max_thread_id {};
};

extern ManuallyInit<Process> KERNEL_PROCESS;
