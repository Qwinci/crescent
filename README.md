# Crescent
## An experimental opinionated kernel

### Todo
- [x] Basic logging
- [x] Memory
	- [x] Page frame allocator
	- [x] Page table manager
	- [x] General purpose memory allocator
- [ ] Interrupt handling
	- [ ] APIC
	- [x] Exceptions
	- [x] IO-APIC
- [ ] Elf loading from modules
- [ ] Basic Round-Robin scheduler
- [ ] ACPI
	- [ ] FADT
	- [x] MADT
- [ ] Filesystems
	- [ ] FAT32
- [ ] Drivers
	- [ ] AHCI SATA
	- [ ] NVMe
	- [ ] PS2
		- [ ] Keyboard
		- [ ] Mouse
	- [ ] Generic PCI device interface
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
	- [ ] SMP