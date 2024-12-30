#pragma once
#include <cstdint>
#include <vector>
#include <memory>

struct PciAddress {
	uint8_t bus;
	uint8_t dev;
	uint8_t func;

	constexpr bool operator==(const PciAddress& other) const = default;
};

struct PciConfigSpace {
	explicit PciConfigSpace(bool pcie)
		: ptr {std::make_unique<uint8_t[]>(pcie ? 0x1000 : 0x100)} {
		memset(ptr.get(), 0xFF, pcie ? 0x1000 : 0x100);

		set_bar(0, 0);
		set_bar(1, 0);
		set_bar(2, 0);
		set_bar(3, 0);
		set_bar(4, 0);
		set_bar(5, 0);
	}

	void write8(uint16_t offset, uint8_t value) {
		ptr[offset] = value;
	}

	void write16(uint16_t offset, uint16_t value) {
		memcpy(&ptr[offset], &value, 2);
	}

	void write32(uint16_t offset, uint32_t value) {
		memcpy(&ptr[offset], &value, 4);
	}

	[[nodiscard]] uint32_t read32(uint16_t offset) const {
		uint32_t value;
		memcpy(&value, &ptr[offset], 4);
		return value;
	}

	void set_device(uint16_t vendor, uint16_t device) {
		write16(0, vendor);
		write16(2, device);
	}

	void set_class(uint8_t clazz, uint8_t subclass, uint8_t prog_if, uint8_t rev) {
		write8(8, rev);
		write8(9, prog_if);
		write8(10, subclass);
		write8(11, clazz);
	}

	void set_type(uint8_t type) {
		write8(0xE, 0);
	}

	void set_subsystem(uint16_t vendor, uint16_t id) {
		write16(0x2C, vendor);
		write16(0x2E, id);
	}

	void set_bar(uint8_t bar, uint32_t value) {
		write32(0x10 + bar * 4, value);
	}

	[[nodiscard]] uint32_t get_bar(uint8_t bar) const {
		return read32(0x10 + bar * 4);
	}

	void set_expansion_rom(uint32_t value) {
		write32(0x30, value);
	}

	[[nodiscard]] uint32_t get_expansion_rom() const {
		return read32(0x30);
	}

	std::unique_ptr<uint8_t[]> ptr;
	void* bars[7] {};
	uint32_t bar_sizes[7] {};
	uint32_t bar_allocated[7] {};
};

struct PciDevice {
	PciDevice(PciAddress addr, bool pcie) : addr {addr}, config {pcie} {}

	PciAddress addr;
	PciConfigSpace config;
};

void pci_add_device(std::unique_ptr<PciDevice> device);
uint32_t pci_config_read(PciAddress addr, uint16_t offset, uint8_t size);
void pci_config_write(PciAddress addr, uint16_t offset, uint32_t value, uint8_t size);

extern std::vector<std::unique_ptr<PciDevice>> PCI_DEVICES;
