; X86Task* x86_switch_task(X86Task* old_task, X86Task* new_task);
global x86_switch_task:function (x86_switch_task.end - x86_switch_task)

struc X86Task
	.self: resq 1
	.rsp: resq 1
endstruc

x86_switch_task:
	push rbx
	push rbp
	push r12
	push r13
	push r14
	push r15

	mov qword [rdi + X86Task.rsp], rsp
	mov rsp, [rsi + X86Task.rsp]

	mov rax, rdi

	pop r15
	pop r14
	pop r13
	pop r12
	pop rbp
	pop rbx
	ret
.end: