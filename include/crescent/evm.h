#ifndef CRESCENT_EVM_H
#define CRESCENT_EVM_H

#include <stdint.h>

#ifdef __x86_64__

typedef enum EvmExitReason {
	EVM_EXIT_REASON_VM_ENTER_FAILED,
	EVM_EXIT_REASON_HALT,
	EVM_EXIT_REASON_IO_IN,
	EVM_EXIT_REASON_IO_OUT,
	EVM_EXIT_REASON_MMIO_READ,
	EVM_EXIT_REASON_MMIO_WRITE,
	EVM_EXIT_REASON_CPUID,
	EVM_EXIT_REASON_TRIPLE_FAULT
} EvmExitReason;

typedef union EvmExitState {
	struct {
		uint16_t port;
		uint8_t size;
		uint32_t ret_value;
	} io_in;

	struct {
		uint16_t port;
		uint8_t size;
		uint32_t value;
	} io_out;

	struct {
		uint64_t guest_phys_addr;
		uint64_t ret_value;
		uint8_t size;
	} mmio_read;

	struct {
		uint64_t guest_phys_addr;
		uint64_t value;
		uint8_t size;
	} mmio_write;
} EvmExitState;

typedef struct EvmSegmentRegister {
	uint64_t base;
	uint16_t selector;
	uint16_t limit;
} EvmSegmentRegister;

typedef struct EvmGuestState {
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rbp;
	uint64_t rsp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;

	uint64_t rip;
	uint64_t rflags;

	uint64_t cr0;
	uint64_t cr3;
	uint64_t cr4;

	EvmSegmentRegister es;
	EvmSegmentRegister cs;
	EvmSegmentRegister ss;
	EvmSegmentRegister ds;
	EvmSegmentRegister fs;
	EvmSegmentRegister gs;
	EvmSegmentRegister ldtr;
	EvmSegmentRegister tr;
	EvmSegmentRegister gdtr;
	EvmSegmentRegister idtr;

	EvmExitReason exit_reason;
	EvmExitState exit_state;
} EvmGuestState;

typedef enum EvmStateBits {
	EVM_STATE_BITS_NONE,
	EVM_STATE_BITS_GP_REGS = 1 << 0,
	EVM_STATE_BITS_RIP = 1 << 1,
	EVM_STATE_BITS_RSP = 1 << 2,
	EVM_STATE_BITS_RFLAGS = 1 << 3,
	EVM_STATE_BITS_SEG_REGS = 1 << 4,
	EVM_STATE_BITS_CONTROL_REGS = 1 << 5,
	EVM_STATE_BITS_ALL = 1 << 0 | 1 << 1 | 1 << 2 | 1 << 3 | 1 << 4 | 1 << 5
} EvmStateBits;

typedef enum EvmIrqType {
	EVM_IRQ_TYPE_EXCEPTION,
	EVM_IRQ_TYPE_IRQ
} EvmIrqType;

typedef struct EvmIrqInfo {
	EvmIrqType type;
	uint32_t irq;
	uint32_t error;
} EvmIrqInfo;

#else

struct EvmGuestState;
typedef enum {
	EVM_STATE_BITS_NONE
} EvmStateBits;
typedef struct EvmIrqInfo EvmIrqInfo;

#endif

#endif
