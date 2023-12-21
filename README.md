# Crescent
## A custom OS distribution
Crescent is a custom operating system distribution,
that is going to come with its own set of userspace applications
and a desktop environment.

This repository hosts the Crescent kernel and apps.

### Features
- Multithreaded pre-emptive custom kernel
- A set of custom userspace applications including a terminal,
file manager, desktop environment, browser, media player and some games.
(**TODO, currently the only userspace application is a dummy echo "terminal"**)
- Works on real hardware and supports different kinds of devices
- Architectures supported: X86-64 (AArch64 support is planned)

### Building the full distribution

Building the whole distribution is best done using [xbstrap](https://github.com/managarm/xbstrap), the required tools for that are:
- C/C++ compiler
- xbstrap
- CMake/Meson/Ninja/Make depending on which packages are built
#### Steps
Downloads the prerequisites once by running:
```bash
xbstrap prereqs xbps
``````
Clone the [crescent bootstrap repository](https://github.com/Qwinci/crescent-bootstrap),
and proceed with the instructions below in that directory.

Initialize the build directory:
```bash
mkdir build
cd build
xbstrap init ..
```

Installing packages is done using xbstrap install:
```bash
# install optinally accepts --rebuild argument for rebuilding the package
xbstrap install <name>
```
There is a set of meta-packages which you can use to build preselected sets of packages:
- `base`: includes the kernel and custom apps

Creating a bios/EFI bootable image:
```bash
# Create an empty image (usually only needed on a clean build)
xbstrap run create-image
# Sync the sysroot to the image
xbstrap run update-image
```
Booting the image in Qemu:
```bash
# Commands can be chained, for an example you can run
# `xbstrap run update-image qemu`
# to both update the image and launch qemu
xbstrap run qemu
```

### Building the plain kernel
It is also possible build the plain Crescent kernel, for that you need following tools:
- C compiler
- CMake
- Ninja (not strictly needed, but the build commands assume Ninja is used)
#### Steps
Note: If you want to cross-compile to a different architecture,
then refer to CMake documentation on toolchain files and add the appropriate flags to below commands.
- `mkdir build`
- `cd build`
- `cmake -G Ninja ../kernel`
- `ninja`

### Todo
- [x] Basic logging
- [x] Memory
	- [x] Page frame allocator
	- [x] Page table manager
	- [x] General purpose memory allocator
- [x] Interrupt handling
	- [x] APIC
	- [x] Exceptions
	- [x] IO-APIC
- [ ] Kernel module loading
- [x] Per-cpu multilevel feedback queue scheduler
- [ ] ACPI
	- [x] FADT
	- [x] MADT
    - [x] MCFG
    - [ ] Power management
- [ ] Filesystems
	- [ ] FAT32
    - [x] Ext4 read
    - [ ] Ext4 write
- [ ] Drivers
	- [x] SMP
	- [ ] AHCI SATA
	- [x] NVMe
	- [x] PS2
		- [x] Keyboard
		- [ ] Mouse
	- [x] Generic PCI device interface
    - [ ] Ethernet
  		- [ ] Realtek RTL8169
	- [ ] USB
		- [ ] Generic HCI interface abstraction
		- [ ] HCI
			- [ ] UHCI
			- [ ] OHCI
			- [ ] EHCI
			- [ ] XHCI
		- [ ] Keyboard
		- [ ] Mouse
		- [ ] Mass storage
