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
  (**TODO, currently the only userspace application is a very basic desktop environment**)
- Works on real hardware and supports different kinds of devices
- Architectures supported: X86-64, AArch64 (it lacks some critical features right now though)

### Building the kernel and apps
The required tools for building are:
- C/C++ compiler
- CMake
- Ninja (or Make, but the steps below assume Ninja is used)
#### Steps
Note: If you want to cross-compile to a different architecture,
there are some pre-defined cmake toolchain files in `cmake` folder
that you can use by adding `-DCMAKE_TOOLCHAIN_FILE=<path to toolchain file>`
to the cmake commandline.
If there isn't an existing toolchain for the target that you want to build for
then refer to the existing toolchains and CMake documentation on toolchain files
and create a new one.
- `mkdir build`
- `cd build`
- `cmake -G Ninja ..`
- `ninja`

There are also different targets that may be useful:

`run` target for running the kernel in Qemu
- `ninja run`

`debug` target that runs the kernel in qemu and also launches remote gdb
- `ninja debug`
- `gdb bin/crescent`
- in gdb: `target remote localhost:1234`

`boot.img` target on AArch64 to create an android boot image (needs `mkbootimg`)
- `ninja boot.img`

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
    - [x] Reboot + shutdown
    - [ ] Power management
- [ ] Filesystems
    - [x] TAR initramfs
    - [ ] FAT32
    - [ ] Ext4 read
    - [ ] Ext4 write
- [ ] Drivers
    - [x] SMP
    - [ ] AHCI SATA
    - [ ] NVMe
    - [ ] Audio
        - [ ] Intel High Definition Audio (mostly done)
    - [x] PS2
        - [x] Keyboard
        - [x] Mouse
    - [x] Generic PCI device interface
    - [x] Ethernet
        - [x] Realtek RTL8169/RTL8139
    - [ ] Network stack
        - [x] Ethernet
        - [x] ARP
        - [x] IPv4
        - [ ] IPv6
        - [ ] UDP v4
        - [ ] UDP v6
        - [ ] TCP v4 (partially done)
        - [ ] TCP v6
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
- [ ] Apps
  - [ ] Desktop environment (very basic one with almost no functionality is done)
  - [ ] Web browser
  - [ ] Terminal
  - [ ] File manager
  - [ ] Media player
