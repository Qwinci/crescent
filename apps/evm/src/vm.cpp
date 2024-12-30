#include "vm.hpp"
#include "sys.hpp"
#include "chipset.hpp"
#include "pci.hpp"
#include <cassert>
#include <stdio.h>

static void qemu_debug_write(void* arg, uint16_t offset, uint32_t value, uint8_t size) {
	fputc(static_cast<char>(value), stdout);
}

struct PciData {
	PciAddress addr;
	uint8_t offset;
	bool enable;
};
PciData PCI_DATA {};

static uint32_t pci_cmd_read(void* arg, uint16_t offset, uint8_t size) {
	return PCI_DATA.enable << 31;
}

static void pci_cmd_write(void* arg, uint16_t offset, uint32_t value, uint8_t size) {
	assert(offset == 0);
	assert(size == 4);
	PCI_DATA.offset = value;
	PCI_DATA.addr.func = value >> 8 & 0b111;
	PCI_DATA.addr.dev = value >> 11 & 0b11111;
	PCI_DATA.addr.bus = value >> 16;
	PCI_DATA.enable = value & 1 << 31;
}

static uint32_t pci_data_read(void*, uint16_t offset, uint8_t size) {
	if (!PCI_DATA.enable) {
		return 0xFFFFFFFF;
	}

	return pci_config_read(PCI_DATA.addr, PCI_DATA.offset + offset, size);
}

static void pci_data_write(void*, uint16_t offset, uint32_t value, uint8_t size) {
	if (!PCI_DATA.enable) {
		return;
	}

	pci_config_write(PCI_DATA.addr, PCI_DATA.offset + offset, value, size);
}

size_t MEMORY_SIZE;

Vm::Vm(uint32_t* fb, uint32_t fb_size) {
	VM = this;

	add_io_region({
		.base = 0x402,
		.size = 1,
		.read = [](void* arg, uint16_t offset, uint8_t size) -> uint32_t {
			return 0xE9;
		},
		.write = qemu_debug_write,
		.arg = nullptr
	});

	add_io_region({
		.base = 0xCF8,
		.size = 4,
		.read = pci_cmd_read,
		.write = pci_cmd_write,
		.arg = nullptr
	});
	add_io_region({
		.base = 0xCFC,
		.size = 4,
		.read = pci_data_read,
		.write = pci_data_write,
		.arg = nullptr
	});

	CrescentHandle bios_handle;
	auto err = sys_open(bios_handle, "/bios.bin", sizeof("/bios.bin") - 1, 0);
	assert(err == 0);

	CrescentStat bios_stat {};
	err = sys_stat(bios_handle, bios_stat);
	assert(err == 0);

	void* bios_mem = nullptr;
	err = sys_map(&bios_mem, (bios_stat.size + 0xFFF) & ~0xFFF, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
	assert(err == 0);
	err = sys_read(bios_handle, bios_mem, bios_stat.size, nullptr);
	assert(err == 0);
	sys_close_handle(bios_handle);

	CrescentHandle evm;
	err = sys_evm_create(evm);
	assert(err == 0);
	CrescentHandle vcpu;
	EvmGuestState* state;
	err = sys_evm_create_vcpu(evm, vcpu, &state);
	assert(err == 0);

	handle = evm;

	chipset_init(this, fb, fb_size);

	vcpus.push_back({
		.handle = vcpu,
		.state = state
	});

	void* low_mem = nullptr;
	err = sys_map(&low_mem, 0xA0000, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
	assert(err == 0);

	MEMORY_SIZE = 1024 * 1024 * 64;

	void* high_mem = nullptr;
	err = sys_map(&high_mem, 0xF00000 + MEMORY_SIZE, CRESCENT_PROT_READ | CRESCENT_PROT_WRITE);
	assert(err == 0);

	err = sys_evm_map(evm, 0, low_mem, 0xA0000);
	assert(err == 0);
	err = sys_evm_map(evm, 0xC0000, bios_mem, (bios_stat.size + 0xFFF) & ~0xFFF);
	assert(err == 0);
	err = sys_evm_map(evm, 0x100000, high_mem, 0xF00000 + MEMORY_SIZE);
	assert(err == 0);

	state->rip = 0;
	state->cs.base = 0xFFFF0;

	err = sys_evm_vcpu_write_state(vcpu, EVM_STATE_BITS_RIP | EVM_STATE_BITS_SEG_REGS);
	assert(err == 0);
}

Vm::~Vm() {
	sys_close_handle(handle);
}

void Vm::run() {
	ArchInfo info {};
	auto status = sys_get_arch_info(&info);
	assert(status == 0);

	for (int i = 0; i < 40000; ++i) {
		uint64_t start = __rdtsc();
		bool continue_run = true;
		for (auto& vcpu : vcpus) {
			int err = sys_evm_vcpu_run(vcpu.handle);
			assert(err == 0);
			continue_run &= handle_vm_exit(vcpu);
		}

		uint64_t end = __rdtsc();

		chipset_update_timers(end - start, info);

		if (!continue_run) {
			break;
		}
	}

	sys_evm_vcpu_trigger_irq(
		vcpus[0].handle,
		{
			.type = EVM_IRQ_TYPE_IRQ,
			.irq = 8,
			.error = 0
		});

	for (int i = 0; i < 1000; ++i) {
		uint64_t start = __rdtsc();
		bool continue_run = true;
		for (auto& vcpu : vcpus) {
			int err = sys_evm_vcpu_run(vcpu.handle);
			assert(err == 0);
			continue_run &= handle_vm_exit(vcpu);
		}

		uint64_t end = __rdtsc();

		chipset_update_timers(end - start, info);

		if (!continue_run) {
			break;
		}
	}
}

bool Vm::handle_vm_exit(VirtualCpu& vcpu) {
	auto state = vcpu.state;

	if (state->exit_reason == EVM_EXIT_REASON_HALT) {
		return false;
	}
	else if (state->exit_reason == EVM_EXIT_REASON_IO_IN) {
		auto& io = state->exit_state.io_in;
		bool found = false;
		for (auto& region : io_regions) {
			if (io.port >= region.base && io.port < region.base + region.size) {
				if (!region.read) {
					printf("region doesn't handle read from port 0x%x\n", io.port);
				}
				io.ret_value = region.read(region.arg, io.port - region.base, io.size);
				found = true;
				break;
			}
		}

		if (!found) {
			printf("unhandled io in 0x%x (size %d)\n", io.port, io.size);
		}
	}
	else if (state->exit_reason == EVM_EXIT_REASON_IO_OUT) {
		auto& io = state->exit_state.io_out;
		bool found = false;
		for (auto& region : io_regions) {
			if (io.port >= region.base && io.port < region.base + region.size) {
				if (!region.write) {
					printf("region doesn't handle write to port 0x%x\n", io.port);
				}
				region.write(region.arg, io.port - region.base, io.value, io.size);
				found = true;
				break;
			}
		}

		if (!found) {
			printf("unhandled io out %x %x (size %d)\n", io.port, io.value, io.size);
		}
	}
	else if (state->exit_reason == EVM_EXIT_REASON_MMIO_READ) {
		auto& mmio = state->exit_state.mmio_read;
		bool found = false;
		for (auto& region : mmio_regions) {
			if (mmio.guest_phys_addr >= region.base && mmio.guest_phys_addr < region.base + region.size) {
				if (!region.read) {
					printf("region doesn't handle read from address 0x%lx\n", mmio.guest_phys_addr);
				}
				mmio.ret_value = region.read(region.arg, mmio.guest_phys_addr - region.base, mmio.size);
				found = true;
				break;
			}
		}

		if (!found) {
			printf("unhandled mmio read %lx\n", state->exit_state.mmio_read.guest_phys_addr);
		}
	}
	else if (state->exit_reason == EVM_EXIT_REASON_MMIO_WRITE) {
		auto& mmio = state->exit_state.mmio_write;
		bool found = false;
		for (auto& region : mmio_regions) {
			if (mmio.guest_phys_addr >= region.base && mmio.guest_phys_addr < region.base + region.size) {
				if (!region.write) {
					printf("region doesn't handle write to address 0x%lx\n", mmio.guest_phys_addr);
				}
				region.write(region.arg, mmio.guest_phys_addr - region.base, mmio.value, mmio.size);
				found = true;
				break;
			}
		}

		if (!found) {
			printf("unhandled mmio write %lx %lx (size %d)\n", mmio.guest_phys_addr, mmio.value, mmio.size);
		}
	}
	else if (state->exit_reason == EVM_EXIT_REASON_CPUID) {
		uint32_t eax;
		uint32_t ebx;
		uint32_t ecx;
		uint32_t edx;
		asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(state->rax), "c"(state->rcx));
		if (state->rax == 0x40000000) {
			state->rax = eax;
			state->rbx = 'c' | 'r' << 8 | 'e' << 16 | 's' << 24;
			state->rcx = 'c' | 'e' << 8 | 'n' << 16 | 't' << 24;
			state->rdx = 'e' | 'v' << 8 | 'm' << 16 | '\0' << 24;
		}
		else {
			state->rax = eax;
			state->rbx = ebx;
			state->rcx = ecx;
			state->rdx = edx;
		}
	}
	else if (state->exit_reason == EVM_EXIT_REASON_TRIPLE_FAULT) {
		printf("triple fault\n");
		exit(1);
	}
	else {
		printf("unhandled exit reason: %d\n", state->exit_reason);
		return false;
	}

	return true;
}

void Vm::add_io_region(IoRegion region) {
	io_regions.push_back(region);
}

void Vm::add_mmio_region(MmioRegion region) {
	mmio_regions.push_back(region);
}

void Vm::map_mem(size_t guest, void* mem, size_t size) {
	size = (size + 0xFFF) & ~0xFFF;
	printf("map %lx (size %zx) -> %p\n", guest, size, mem);
	auto err = sys_evm_map(handle, guest, mem, size);
	assert(err == 0);
}

void Vm::unmap_mem(size_t guest, size_t size) {
	size = (size + 0xFFF) & ~0xFFF;
	auto err = sys_evm_unmap(handle, guest, size);
	assert(err == 0);
}

uint32_t Vm::map_mem_alloc(void* mem, size_t size) {
	size = (size + 0xFFF) & ~0xFFF;
	auto addr = 0xC0000000 + mapped_mmio_size;
	auto err = sys_evm_map(handle, addr, mem, size);
	assert(err == 0);

	mapped_mmio_size += size;
	return addr;
}

Vm* VM;
