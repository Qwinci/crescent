global switch_task

struc Process
	.rsp: resq 1
	.kernel_rsp: resq 1
	.map: resq 1
	.next: resq 1
	.state: resq 1
	.user_cpu_time_used_ns: resq 1
	.last_user_timestamp: resq 1
	.kernel_cpu_time_used_ns: resq 1
	.last_kernel_timestamp: resq 1
	.sleep_end: resq 1
endstruc

%define STATE_READY_TO_RUN 0
%define STATE_RUNNING 1

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
	.apic_freq: resq 1
	.tsc_freq: resq 1
	.tss: resb TSS_SIZE
endstruc

extern current_task
extern ready_to_run_tasks
extern ready_to_run_tasks_end
extern delay_task_switch
extern task_switch_delayed
switch_task:
	cmp qword [delay_task_switch], 0
	je .do_switch
	mov byte [task_switch_delayed], 1
	ret
.do_switch:
	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	mov rax, [current_task]

	; if current task is null then don't append it to ready_to_run
	test rax, rax
	je .continue

	; save rsp if there is a current task
    mov qword [rax + Process.rsp], rsp

	; if current tasks state is not running don't append it to ready_to_run
	cmp qword [rax + Process.state], STATE_RUNNING
	jne .continue

	; set the current tasks state to ready to run and append it to the list of ready to run tasks
	mov qword [rax + Process.state], STATE_READY_TO_RUN

	mov rbx, [ready_to_run_tasks_end]
	mov qword [ready_to_run_tasks_end], rax
	mov qword [rax + Process.next], rbx

	; if ready_to_run_tasks is null then move ready_to_run_tasks_end to it
	cmp qword [ready_to_run_tasks], 0
	jne .continue

	mov qword [ready_to_run_tasks], rax
.continue:
	mov qword [rdi + Process.state], STATE_RUNNING

	; current_task = next_task
	mov qword [current_task], rdi

	; load registers
	mov rsp, [rdi + Process.rsp]
	mov r8, [rdi + Process.map]

	; get current cpu local
	mov rax, [gs:0]

	mov rsi, [rdi + Process.kernel_rsp]
	mov dword [rax + CpuLocal.tss + Tss.rsp0_low], esi
	shl rsi, 32
	mov dword [rax + CpuLocal.tss + Tss.rsp0_high], esi

	mov r9, cr3

	cmp r9, r8
	je .done
	mov cr3, r8

.done:
	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx
	ret