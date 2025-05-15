
asm(R"(
.pushsection .text
.globl smp_trampoline_start
.globl smp_trampoline_start32
.globl smp_trampoline_end

.section .data
.code16
smp_trampoline_start:
	cli
	cld

	mov %cs:(boot_info.page_map - smp_trampoline_start), %eax
	mov %eax, %cr3

	mov %cr4, %eax
	// enable pae
	or $(1 << 5), %eax
	mov %eax, %cr4

	// enable long mode and nx
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8 | 1 << 11), %eax
	wrmsr

	mov %cr0, %eax
	// enable paging and protected mode
	or $(1 << 31 | 1 << 0), %eax
	mov %eax, %cr0

	mov %cs, %ebx
	shl $4, %ebx
	lea gdt - smp_trampoline_start(%ebx), %eax
	mov %eax, gdtr - smp_trampoline_start + 2(%ebx)

	lgdt %cs:(gdtr - smp_trampoline_start)

	lea long_mode - smp_trampoline_start(%ebx), %eax
	mov %eax, farjmp - smp_trampoline_start(%ebx)

	ljmpl *%cs:(farjmp - smp_trampoline_start)

farjmp:
	.long 0
	.word 0x8

.align 8
.code32
smp_trampoline_start32:
	cli
	cld

	// this gets patched at runtime to the address of the trampoline
	mov $0xCAFEBABE, %ebx

	mov (boot_info.page_map - smp_trampoline_start)(%ebx), %eax
	mov %eax, %cr3

	mov %cr4, %eax
	// enable pae
	or $(1 << 5), %eax
	mov %eax, %cr4

	// enable long mode and nx
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8 | 1 << 11), %eax
	wrmsr

	mov %cr0, %eax
	// enable paging and protected mode
	or $(1 << 31 | 1 << 0), %eax
	mov %eax, %cr0

	lea (gdt - smp_trampoline_start)(%ebx), %eax
	mov %eax, (gdtr - smp_trampoline_start + 2)(%ebx)

	lgdt (gdtr - smp_trampoline_start)(%ebx)

	lea (long_mode - smp_trampoline_start)(%ebx), %eax
	mov %eax, (farjmp - smp_trampoline_start)(%ebx)

	ljmpl *(farjmp - smp_trampoline_start)(%ebx)

.align 8
.code64
long_mode:

	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %ss
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	mov boot_info.stack - smp_trampoline_start(%ebx), %rsp
	xor %ebp, %ebp
	mov boot_info.arg - smp_trampoline_start(%ebx), %rdi
	jmp *boot_info.entry - smp_trampoline_start(%ebx)

gdt:
	// null
	.quad 0
	// kernel code
	.quad 0xAF9A000000FFFF
	// kernel data
	.quad 0xCF92000000FFFF
gdtr:
	.word 24 - 1
	.long 0
.align 16
boot_info:
	boot_info.stack: .quad 0
	boot_info.entry: .quad 0
	boot_info.arg: .quad 0
	boot_info.page_map: .long 0

smp_trampoline_end:

.code64
.popsection
)");
