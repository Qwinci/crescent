#pragma once
#include "types.h"

typedef enum : u8 {
	IO_APIC_DELIVERY_MODE_FIXED = 0,
	IO_APIC_DELIVERY_MODE_LOW_PRIO = 1,
	IO_APIC_DELIVERY_MODE_SMI = 0b10,
	IO_APIC_DELIVERY_MODE_NMI = 0b100,
	IO_APIC_DELIVERY_MODE_INIT = 0b101,
	IO_APIC_DELIVERY_MODE_EXT_INT = 0b111
} IoApicDeliveryMode;

typedef enum : u8 {
	IO_APIC_DEST_PHYSICAL = 0,
	IO_APIC_DEST_LOGICAL = 1
} IoApicDest;

typedef enum : u8 {
	IO_APIC_PIN_ACTIVE_HIGH = 0,
	IO_APIC_PIN_ACTIVE_LOW = 1
} IoApicPinPolarity;

typedef enum : u8 {
	IO_APIC_TRIGGER_EDGE = 0,
	IO_APIC_TRIGGER_LEVEL = 1
} IoApicTriggerMode;

#define MAX_IO_APIC_COUNT 16

typedef struct {
	usize base;
	u32 int_base;
	u32 used_irqs;
	u8 irq_count;
} IoApic;

typedef struct {
	u8 vec;
	IoApicDeliveryMode delivery_mode;
	IoApicDest dest_mode;
	IoApicPinPolarity pin;
	IoApicTriggerMode trigger;
	u8 mask;
	u8 dest;
} IoApicRedirEntry;

void io_apic_register_isa_irq(u8 irq, IoApicRedirEntry entry);
void io_apic_register_irq(u32 io_apic_irq, IoApicRedirEntry entry);
bool io_apic_is_entry_free(u32 io_apic_irq);
u32 io_apic_get_free_irq();

extern usize io_apic_count;
extern IoApic io_apics[MAX_IO_APIC_COUNT];