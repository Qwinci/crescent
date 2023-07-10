#pragma once

typedef struct Task Task;

void sched_init(bool bsp);
void arch_sched_init(bool bsp);

void sched_switch_from_init(Task* old_task);
void sched_switch_from(Task* old_task, Task* self);
// Precondition: ipl is critical or interrupts are disabled
void sched_with_next(Task* task);

// Precondition: ipl is critical or interrupts are disabled
Task* sched_get_next_task();

void arch_switch_task(Task* self, Task* new_task);
void arch_sched_switch_from(Task* old_task, Task* self);