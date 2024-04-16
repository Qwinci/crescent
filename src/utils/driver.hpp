#pragma once
#include "initializer_list.hpp"

#if __aarch64__
#include "arch/aarch64/dtb.hpp"

struct DtDriver {
	bool (*init)(dtb::Node node, dtb::Node parent) {};
	std::initializer_list<const kstd::string_view> compatible {};
};

struct Driver {
	enum class Type {
		Dt
	} type;
	union {
		DtDriver* dt;
	};
};

#define DT_DRIVER(variable) [[gnu::section(".drivers"), gnu::used]] \
static Driver DRIVER_ ## variable {.type = Driver::Type::Dt, .dt = &(variable)}

#elif __x86_64__
#include "dev/pci.hpp"

struct PciDeviceMatch {
	u16 vendor;
	u16 device;
};

enum class PciMatch {
	Class = 1 << 0,
	Subclass = 1 << 1,
	ProgIf = 1 << 2,
	Device = 1 << 3
};

constexpr PciMatch operator|(PciMatch lhs, PciMatch rhs) {
	return static_cast<PciMatch>(static_cast<int>(lhs) | static_cast<int>(rhs));
}
constexpr bool operator&(PciMatch lhs, PciMatch rhs) {
	return static_cast<int>(lhs) & static_cast<int>(rhs);
}

struct PciDriver {
	bool (*init)(pci::Device& device) {};
	PciMatch match {};
	u8 _class {};
	u8 subclass {};
	u8 prog_if {};
	bool (*fine_match)(pci::Device& device) {};
	std::initializer_list<const PciDeviceMatch> devices {};
};

struct Driver {
	enum class Type {
		Pci
	} type;
	union {
		const PciDriver* pci;
	};
};

#define PCI_DRIVER(variable) [[gnu::section(".drivers"), gnu::used]] \
static Driver DRIVER_ ## variable {.type = Driver::Type::Pci, .pci = &(variable)}

#endif
