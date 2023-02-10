struc Tss
	.reserved1: resw 1
	.iopb: resw 1
	.reserved2: resd 2
	.ist7_high: resd 1
	.ist7_low: resd 1
	.ist6_high: resd 1
	.ist6_low: resd 1
	.ist5_high: resd 1
	.ist5_low: resd 1
	.ist4_high: resd 1
	.ist4_low: resd 1
	.ist3_high: resd 1
	.ist3_low: resd 1
	.ist2_high: resd 1
	.ist2_low: resd 1
	.ist1_high: resd 1
	.ist1_low: resd 1
	.reserved3: resd 2
	.rsp2_high: resd 1
	.rsp2_low: resd 1
	.rsp1_high: resd 1
	.rsp1_low: resd 1
	.rsp0_high: resd 1
	.rsp0_low: resd 1
	.reserved4: resd 1
endstruc

%define TSS_SIZE Tss.reserved4 + 4

struc CpuLocal
	.self: resq 1
	.apic_frequency: resq 1
	.tsc_frequency: resq 1
	.tss: resb TSS_SIZE
	.current_task: resq 1
endstruc