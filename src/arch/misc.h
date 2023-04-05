#pragma once

void arch_hlt();
void arch_spinloop_hint();
void* enter_critical();
void leave_critical(void* flags);