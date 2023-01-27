global switch_task

extern current_task
extern ready_tasks
extern ready_tasks_end
extern sched_lock

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
endstruc

%define STATUS_READY 1
%define STATUS_RUNNING 0

struc Task
	.name: resb 128
	.rsp: resq 1
	.kernel_rsp: resq 1
	.next: resq 1
	.map: resq 1
	.status: resb 1
	.reserved: resb 7
endstruc

switch_task:
	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	mov rax, [current_task]

	mov qword [rax + Task.rsp], rsp

	; if the current task is not running then skip adding it to list of ready tasks
	cmp byte [rax + Task.status], STATUS_RUNNING
	jne .skip_add

	; if ready tasks is empty then skip setting the next field of the last task
	mov rbx, [ready_tasks_end]
	test rbx, rbx
	je .skip_next

	mov qword [rbx + Task.next], rax
.skip_next:
	mov qword [ready_tasks_end], rax

	; if ready tasks was not empty then skip setting ready tasks to the current task
	jne .skip_add

	mov qword [ready_tasks], rax
.skip_add:

	; set the new tasks status to running
	mov byte [rdi + Task.status], STATUS_RUNNING

	; set the current task to the new task
	mov qword [current_task], rdi

	; load new rsp
	mov rsp, [rdi + Task.rsp]

	; check if the task uses the same page map
	mov rcx, cr3
	cmp qword [rdi + Task.map], rcx
	je .same_map
	; if not then load the new one
	mov rdx, [rdi + Task.map]
	mov cr3, rdx

.same_map:
	mov rax, [rdi + Task.kernel_rsp]

	mov rsi, [gs:0]

	mov dword [rsi + CpuLocal.tss + Tss.rsp0_low], eax
	shr rax, 32
	mov dword [rsi + CpuLocal.tss + Tss.rsp0_high], eax

	mov dword [sched_lock], 0

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx
	ret