# Crescent
## An experimental kernel

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
- [ ] Elf loading from modules
- [x] Per-cpu multilevel feedback queue scheduler
- [ ] ACPI
	- [x] FADT
	- [x] MADT
    - [ ] MCFG
- [ ] Filesystems
	- [ ] FAT32
- [ ] Drivers
	- [x] SMP
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
