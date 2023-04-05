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
        )