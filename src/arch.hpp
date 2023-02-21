#pragma once
#include "cpu/cpu.hpp"
#include "fb.hpp"

struct PsfFont;

void arch_init_mem();
void arch_init_cpu_locals();
void arch_init_smp(void (*fn)(u8 id, u32 acpi_id));
void* arch_get_rsdp();
const PsfFont* arch_get_font();
Framebuffer arch_get_framebuffer();
void* arch_get_module(const char* name);
CpuLocal* arch_get_cpu_local();
CpuLocal* arch_get_cpu_local(usize cpu);
void arch_init_usermode();
void arch_sched_after_switch_from(Task* old_task, Task* this_task);
void arch_sched_load_balance_hlt(u8 cpu);
void arch_eoi();