set(CONFIG_MAX_CPUS 24 CACHE STRING "Maximum number of cpus to support")
option(CONFIG_TRACING "Enable verbose trace logging" OFF)

option(BUILD_APPS "Build apps and libraries" ON)

if(ARCH STREQUAL "x86_64")
	set(CONFIG_PCI ON CACHE BOOL "Enable pci support")
else()
	set(CONFIG_PCI OFF)
endif()

if(ARCH STREQUAL "aarch64")
	set(CONFIG_DTB ON CACHE BOOL "Enable dtb support")
else()
	set(CONFIG_DTB OFF)
endif()

option(CONFIG_USB_XHCI "Enable xhci usb support" ON)
if(CONFIG_PCI AND CONFIG_USB_XHCI)
	option(CONFIG_XHCI_PCI "Enable xhci pci support" ON)
endif()

if(CONFIG_DTB AND CONFIG_USB_XHCI)
	option(CONFIG_XHCI_DTB "Enable xhci dtb support" ON)
endif()

option(CONFIG_UFS "Enable ufs support" ON)
if(CONFIG_PCI AND CONFIG_UFS)
	option(CONFIG_UFS_PCI "Enable ufs pci support" ON)
endif()
if(CONFIG_DTB AND CONFIG_UFS)
	option(CONFIG_UFS_DTB "Enable ufs dtb support" ON)
endif()

configure_file(config.hpp.in config.hpp @ONLY)
target_include_directories(crescent PRIVATE "${CMAKE_BINARY_DIR}")
