bits 16
global smp_tramp_start
global smp_tramp_end

SMP_TRAMP_SIZE: equ smp_tramp_end - smp_tramp_start

struc BootInfo
	.entry: resq 1
	.stack: resq 1
	.page_table: resd 1
	.apic_id: resb 1
	.done: resb 1
endstruc

smp_tramp_start:
	cli
	in al, 0x70
	and al, 0x7F
	out 0x70, al
	in al, 0x71

	mov ebp, 0xCAFEBABE

	mov eax, dword [ebp + SMP_TRAMP_SIZE + BootInfo.page_table]
	mov cr3, eax

	mov eax, cr4
	or eax, 1 << 5 | 1 << 7
	mov cr4, eax

	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	; enable paging, protected mode and write protect
	mov eax, cr0
	or eax, 1 << 31 | 1 << 0 | 1 << 16
	mov cr0, eax

	mov eax, ebp
	add eax, gdt - smp_tramp_start
	mov dword [gdt.ptr - smp_tramp_start + 2 + ebp], eax
	lgdt [gdt.ptr - smp_tramp_start + ebp]

	mov eax, ebp
	add eax, long_mode - smp_tramp_start

	push dword 0x8
	push eax
	retfd
gdt:
	.null: dq 0
	.kernel_code:
		dd 0
		db 0, 0x9A, 0xA << 4, 0
	.kernel_data:
		dd 0
		db 0, 0x92, 0xC << 4, 0
	.ptr:
		dw $ - gdt - 1
		dd 0

bits 64
long_mode:
	mov ax, 0x10
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax
	mov rsp, qword [ebp + SMP_TRAMP_SIZE + BootInfo.stack]

	mov byte [ebp + SMP_TRAMP_SIZE + BootInfo.done], 1
	xor edi, edi
	mov dil, byte [ebp + SMP_TRAMP_SIZE + BootInfo.apic_id]
	mov rax, qword [ebp + SMP_TRAMP_SIZE + BootInfo.entry]
	xor ebp, ebp
	jmp rax
smp_tramp_end: