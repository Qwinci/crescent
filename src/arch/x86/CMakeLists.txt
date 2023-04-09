target_sources(crescent PRIVATE
        start.c
        utils.c
        std_mem.c
        con.c
        mod.c
        map.c
        mem.c
        interrupts/interrupts.c
        interrupts/idt.c
        interrupts/int_stubs.asm
        interrupts/gdt.c
        interrupts/gdt.asm
        interrupts/exceptions.c
        smp.c
        cpu.c
        cpuid.c
        user_map.c
        )

add_subdirectory(dev)
add_subdirectory(sched)