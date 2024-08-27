#pragma once
#include "initializer_list.hpp"
#include "string_view.hpp"
#include "config.hpp"
#include "dev/usb/device.hpp"
#include "utils/flags_enum.hpp"

enum class DriverType {
	Pci,
	Dt,
	Usb
};

enum class InitStatus {
	Success,
	Defer,
	Error
};

#if CONFIG_DTB

#include "arch/aarch64/kernel_dtb.hpp"

struct DtDriver {
	kstd::string_view name {};
	InitStatus (*init)(DtbNode& node) {};
	std::initializer_list<const kstd::string_view> compatible {};
	std::initializer_list<const kstd::string_view> depends {};
	std::initializer_list<const kstd::string_view> provides {};
};

#define DT_DRIVER(variable) [[gnu::section(".drivers"), gnu::used]] \
static constexpr Driver DRIVER_ ## variable {.type = DriverType::Dt, .dt = &(variable)}

#endif

#if CONFIG_PCI
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

FLAGS_ENUM(PciMatch);

struct PciDriver {
	kstd::string_view name {};
	InitStatus (*init)(pci::Device& device) {};
	PciMatch match {};
	u8 _class {};
	u8 subclass {};
	u8 prog_if {};
	bool (*fine_match)(pci::Device& device) {};
	std::initializer_list<const PciDeviceMatch> devices {};
};

#define PCI_DRIVER(variable) [[gnu::section(".drivers"), gnu::used]] \
static constexpr Driver DRIVER_ ## variable {.type = DriverType::Pci, .pci = &(variable)}

#endif

enum class UsbMatch {
	InterfaceClass
};

FLAGS_ENUM(UsbMatch);

struct UsbDriver {
	kstd::string_view name {};
	bool (*init)(const usb::AssignedDevice& device) {};
	UsbMatch match {};
	u8 interface_class {};
	bool (*fine_match)(const UniquePhysical& config, const usb::DeviceDescriptor& device_desc) {};
};

#define USB_DRIVER(variable) [[gnu::section(".drivers"), gnu::used]] \
static constexpr Driver DRIVER_ ## variable {.type = DriverType::Usb, .usb = &(variable)}

struct Driver {
	DriverType type;
	union {
#if CONFIG_PCI
		const PciDriver* pci;
#endif
#if CONFIG_DTB
		const DtDriver* dt;
#endif
		const UsbDriver* usb;
	};
};
