#pragma once
#include "types.h"

typedef enum {
	IRQ_ACK,
	IRQ_NACK
} IrqStatus;

typedef IrqStatus (*IrqHandlerFn)(void* ctx, void* userdata);

typedef struct IrqHandler {
	IrqHandlerFn fn;
	void* userdata;

	bool can_be_shared;
	struct IrqHandler* prev;
	struct IrqHandler* next;
} IrqHandler;

typedef enum : u8 {
	IPL_NORMAL,
	IPL_DEV,
	IPL_TIMER,
	IPL_INTER_CPU,
	IPL_CRITICAL
} Ipl;

typedef enum {
	IRQ_INSTALL_STATUS_SUCCESS,
	IRQ_INSTALL_STATUS_ALREADY_EXISTS
} IrqInstallStatus;

typedef enum {
	IRQ_REMOVE_STATUS_SUCCESS,
	IRQ_REMOVE_STATUS_NOT_EXIST
} IrqRemoveStatus;

typedef enum {
	IRQ_INSTALL_FLAG_NONE = 0,
	IRQ_INSTALL_SHARED = 1 << 0
} IrqInstallFlags;

IrqInstallStatus arch_irq_install(u32 irq, IrqHandler* persistent_handler, IrqInstallFlags flags);
IrqRemoveStatus arch_irq_remove(u32 irq, IrqHandler* persistent_handler);
u32 arch_irq_alloc_generic(Ipl ipl, u32 count, IrqInstallFlags flags);
void arch_irq_dealloc_generic(u32 irq, u32 count, IrqInstallFlags flags);

Ipl arch_ipl_set(Ipl ipl);