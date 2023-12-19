# Crescent
## A custom OS distribution
Crescent is a custom operating system distribution,
that is going to come with its own set of userspace applications
and a desktop environment.

### Features
- Multithreaded pre-emptive custom kernel
- A set of custom userspace applications including a terminal,
file manager, desktop environment, browser, media player and some games.
(**TODO, currently the only userspace application is a dummy echo "terminal"**)
- Works on real hardware and supports different kinds of devices
- Architectures supported: X86-64 (AArch64 support is planned)

### Building
To build a Qemu bootable Crescent ISO you need the following tools:
- CMake
- Ninja (not strictly needed, but the build commands assume Ninja is used)
- Meson
- xorriso
- git
- Qemu (for running it)
#### Steps
Note: If you want to cross-compile to a different architecture,
then refer to CMake documentation on toolchain files and add the appropriate flags to below commands.
- `mkdir build`
- `cd build`
- `cmake -G Ninja ..`
- `ninja`

There is also a run target for directly running Qemu,
which you can use by running the following command in the build directory:
- `ninja run`

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
