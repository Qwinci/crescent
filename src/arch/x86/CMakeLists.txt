target_sources(crescent PRIVATE
        start.c
        utils.c
        mem/std_mem.c
        con.c
        mod.c
        mem/map.c
        mem/mem.c
        interrupts/interrupts.c
        interrupts/idt.c
        interrupts/int_stubs.asm
        interrupts/gdt.c
        interrupts/gdt.asm
        interrupts/exceptions.c
        smp.c
        cpu.c
        cpuid.c
        mem/user_map.c
        acpi/fadt.c
        acpi/madt.c
        )

add_subdirectory(dev)
add_subdirectory(sched)