target_sources(crescent PRIVATE
        start.c
        utils.c
        mem/std_mem.c
        con.c
        mod.c
        mem/map.c
        mem/mem.c
		mem/misc.S
        interrupts/interrupts.c
        interrupts/idt.c
		interrupts/int_stubs.S
        interrupts/gdt.c
		interrupts/gdt.S
        interrupts/exceptions.c
        interrupts/ipl.c
        smp.c
        cpu.c
        cpuid.c
        mem/user_map.c
        acpi/fadt.c
        acpi/madt.c
		start.S
        )
target_include_directories(crescent PRIVATE include)

add_subdirectory(dev)
add_subdirectory(sched)