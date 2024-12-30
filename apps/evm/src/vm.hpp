#pragma once
#include "crescent/syscalls.h"
#include "crescent/evm.h"
#include <cstdint>
#include <vector>

struct IoRegion {
	uint16_t base;
	uint16_t size;
	uint32_t (*read)(void* arg, uint16_t offset, uint8_t size);
	void (*write)(void* arg, uint16_t offset, uint32_t value, uint8_t size);
	void* arg;
};

struct MmioRegion {
	uint64_t base;
	uint64_t size;

	uint64_t (*read)(void* arg, uint64_t offset, uint8_t size);
	void (*write)(void* arg, uint64_t offset, uint64_t value, uint8_t size);
	void* arg;
};

struct VirtualCpu {
	CrescentHandle handle;
	EvmGuestState* state;
};

struct Vm {
	Vm(uint32_t* fb, uint32_t fb_size);

	~Vm();

	void run();

	bool handle_vm_exit(VirtualCpu& vcpu);

	void add_io_region(IoRegion region);
	void add_mmio_region(MmioRegion region);

	void map_mem(size_t guest, void* mem, size_t size);
	void unmap_mem(size_t guest, size_t size);

	[[nodiscard]] uint32_t map_mem_alloc(void* mem, size_t size);

	std::vector<IoRegion> io_regions;
	std::vector<MmioRegion> mmio_regions;
	std::vector<VirtualCpu> vcpus;

	CrescentHandle handle {};
	size_t mapped_mmio_size {};
};

extern Vm* VM;
